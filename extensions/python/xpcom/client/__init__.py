# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <mhammond@skippinet.com.au> (original author)
#

import os
import new
from xpcom import xpt, COMException, nsError

# Suck in stuff from _xpcom we use regularly to prevent a module lookup
from xpcom._xpcom import IID_nsISupports, IID_nsIClassInfo, IID_nsISupportsString, IID_nsISupportsWeakReference, \
        IID_nsIWeakReference, XPTI_GetInterfaceInfoManager, NS_GetGlobalComponentManager, XPTC_InvokeByIndex

# Attribute names we may be __getattr__'d for, but know we don't want to delegate
# Could maybe just look for startswith("__") but this may screw things for some objects.
_special_getattr_names = ["__del__", "__len__", "__nonzero__"]

_just_int_interfaces = ["nsISupportsPRInt32", "nsISupportsPRInt16", "nsISupportsPRUint32", "nsISupportsPRUint16", "nsISupportsPRUint8", "nsISupportsPRBool"]
_just_long_interfaces = ["nsISupportsPRInt64", "nsISupportsPRUint64"]
_just_float_interfaces = ["nsISupportsDouble", "nsISupportsFloat"]
# When doing a specific conversion, the order we try the interfaces in.
_int_interfaces = _just_int_interfaces + _just_float_interfaces
_long_interfaces = _just_long_interfaces + _just_int_interfaces + _just_float_interfaces
_float_interfaces = _just_float_interfaces + _just_long_interfaces + _just_int_interfaces

method_template = """
def %s(self, %s):
    return XPTC_InvokeByIndex(self._comobj_, %d, (%s, (%s)))
"""
def _MakeMethodCode(method):
    # Build a declaration
    param_no = 0
    param_decls = []
    param_flags = []
    param_names = []
    used_default = 0
    for param in method.params:
        param_no = param_no + 1
        param_name = "Param%d" % (param_no,)
        param_default = ""
        if not param.hidden_indicator and param.IsIn() and not param.IsDipper():
            if param.IsOut() or used_default: # If the param is "inout", provide a useful default for the "in" direction.
                param_default = " = None"
                used_default = 1 # Once we have used one once, we must for the rest!
            param_decls.append(param_name + param_default)
            param_names.append(param_name)

        type_repr = xpt.MakeReprForInvoke(param)
        param_flags.append( (param.param_flags,) +  type_repr )
    sep = ", "
    param_decls = sep.join(param_decls)
    if len(param_names)==1: # Damn tuple reprs.
        param_names = param_names[0] + ","
    else:
        param_names = sep.join(param_names)
    # A couple of extra newlines make them easier to read for debugging :-)
    return method_template % (method.name, param_decls, method.method_index, tuple(param_flags), param_names)

# Keyed by IID, each item is a tuple of (methods, getters, setters)
interface_cache = {}
# Keyed by [iid][name], each item is an unbound method.
interface_method_cache = {}

# Keyed by clsid from nsIClassInfo - everything ever queried for the CID.
contractid_info_cache = {}
have_shutdown = 0

def _shutdown():
    interface_cache.clear()
    interface_method_cache.clear()
    contractid_info_cache.clear()
    global have_shutdown
    have_shutdown = 1

# Fully process the named method, generating method code etc.
def BuildMethod(method_info, iid):
    name = method_info.name
    try:
        return interface_method_cache[iid][name]
    except KeyError:
        pass
    # Generate it.
    assert not (method_info.IsSetter() or method_info.IsGetter()), "getters and setters should have been weeded out by now"
    method_code = _MakeMethodCode(method_info)
    # Build the method - We only build a function object here
    # - they are bound to each instance as needed.
    
##    print "Method Code for %s (%s):" % (name, iid)
##    print method_code
    codeObject = compile(method_code, "<XPCOMObject method '%s'>" % (name,), "exec")
    # Exec the code object
    tempNameSpace = {}
    exec codeObject in globals(), tempNameSpace
    ret = tempNameSpace[name]
    if not interface_method_cache.has_key(iid):
        interface_method_cache[iid] = {}
    interface_method_cache[iid][name] = ret
    return ret

from xpcom.xpcom_consts import XPT_MD_GETTER, XPT_MD_SETTER, XPT_MD_NOTXPCOM, XPT_MD_CTOR, XPT_MD_HIDDEN
FLAGS_TO_IGNORE = XPT_MD_NOTXPCOM | XPT_MD_CTOR | XPT_MD_HIDDEN

# Pre-process the interface - generate a list of methods, constants etc,
# but don't actually generate the method code.
def BuildInterfaceInfo(iid):
    assert not have_shutdown, "Can't build interface info after a shutdown"
    ret = interface_cache.get(iid, None)
    if ret is None:
        # Build the data for the cache.
        method_code_blocks = []
        getters = {}
        setters = {}
        method_infos = {}
        
        interface = xpt.Interface(iid)
        for m in interface.methods:
            flags = m.flags
            if flags & FLAGS_TO_IGNORE == 0:
                if flags & XPT_MD_SETTER:
                    param_flags = map(lambda x: (x.param_flags,) + xpt.MakeReprForInvoke(x), m.params)
                    setters[m.name] = m.method_index, param_flags
                elif flags & XPT_MD_GETTER:
                    param_flags = map(lambda x: (x.param_flags,) + xpt.MakeReprForInvoke(x), m.params)
                    getters[m.name] = m.method_index, param_flags
                else:
                    method_infos[m.name] = m

        # Build the constants.
        constants = {}
        for c in interface.constants:
            constants[c.name] = c.value
        ret = method_infos, getters, setters, constants
        interface_cache[iid] = ret
    return ret

class _XPCOMBase:
    def __cmp__(self, other):
        try:
            other = other._comobj_
        except AttributeError:
            pass
        return cmp(self._comobj_, other)

    def __hash__(self):
        return hash(self._comobj_)

    # See if the object support strings.
    def __str__(self):
        try:
            self._comobj_.QueryInterface(IID_nsISupportsString, 0)
            return str(self._comobj_)
        except COMException:
            return self.__repr__()

    # Try the numeric support.
    def _do_conversion(self, interface_names, cvt):
        iim = XPTI_GetInterfaceInfoManager()
        for interface_name in interface_names:
            iid = iim.GetInfoForName(interface_name).GetIID()
            try:
                prim = self._comobj_.QueryInterface(iid)
                return cvt(prim.data)
            except COMException:
                pass
        raise ValueError, "This object does not support automatic numeric conversion to this type"

    def __int__(self):
        return self._do_conversion(_int_interfaces, int)

    def __long__(self):
        return self._do_conversion(_long_interfaces, long)

    def __float__(self):
        return self._do_conversion(_float_interfaces, float)
    
class Component(_XPCOMBase):
    def __init__(self, ob, iid):
        assert not hasattr(ob, "_comobj_"), "Should be a raw nsIWhatever, not a wrapped one"
        ob_name = None
        if not hasattr(ob, "IID"):
            ob_name = ob
            cm = NS_GetGlobalComponentManager()
            ob = cm.CreateInstanceByContractID(ob)
            assert not hasattr(ob, "_comobj_"), "The created object should be a raw nsIWhatever, not a wrapped one"
        # Keep a reference to the object in the component too
        self.__dict__['_comobj_'] = ob
        # hit __dict__ directly to avoid __setattr__()
        self.__dict__['_interfaces_'] = {} # keyed by IID
        self.__dict__['_interface_names_'] = {} # keyed by IID name
        self.__dict__['_interface_infos_'] = {} # keyed by IID
        self.__dict__['_name_to_interface_name_'] = {}
        self.__dict__['_tried_classinfo_'] = 0

        if ob_name is None:
            ob_name = "<unknown>"
        self.__dict__['_object_name_'] = ob_name
        self.QueryInterface(iid)

    def _build_all_supported_interfaces_(self):
        # Use nsIClassInfo, but don't do it at object construction to keep perf up.
        # Only pay the penalty when we really need it.
        assert not self._tried_classinfo_, "already tried to get the class info."
        self.__dict__['_tried_classinfo_'] = 1
        # See if nsIClassInfo is supported.
        try:
            classinfo = self._comobj_.QueryInterface(IID_nsIClassInfo, 0)
        except COMException:
            classinfo = None
        if classinfo is not None:
            real_cid = classinfo.contractID
            if real_cid:
                self.__dict__['_object_name_'] = real_cid
                contractid_info = contractid_info_cache.get(real_cid)
            else:
                contractid_info = None
            if contractid_info is None:
#               print "YAY - have class info!!", classinfo
                for nominated_iid in classinfo.getInterfaces():
                    # Interface may appear twice in the class info list, so check this here.
                    if not self.__dict__['_interface_infos_'].has_key(nominated_iid):
                        self._remember_interface_info(nominated_iid)
                if real_cid is not None:
                    contractid_info = {}
                    contractid_info['_name_to_interface_name_'] = self.__dict__['_name_to_interface_name_']
                    contractid_info['_interface_infos_'] = self.__dict__['_interface_infos_']
                    contractid_info_cache[real_cid] = contractid_info
            else:
                for key, val in contractid_info.items():
                    self.__dict__[key].update(val)

        self.__dict__['_com_classinfo_'] = classinfo

    def _remember_interface_info(self, iid):
        method_infos, getters, setters, constants = BuildInterfaceInfo(iid)
        # Remember all the names so we can delegate
        assert not self.__dict__['_interface_infos_'].has_key(iid), "Already remembered this interface!"
        self.__dict__['_interface_infos_'][iid] = method_infos, getters, setters, constants
        interface_name = iid.name
        names = self.__dict__['_name_to_interface_name_']
        for name in method_infos.keys(): names[name] = interface_name
        for name in getters.keys(): names[name] = interface_name
        for name in setters.keys(): names[name] = interface_name
        for name in constants.keys():  names[name] = interface_name

    def _make_interface_info(self, ob, iid):
        interface_infos = self._interface_infos_
        assert not self._interfaces_.has_key(iid), "Already have made this interface"
        method_infos, getters, setters, constants = interface_infos[iid]
        new_interface = _Interface(ob, iid, method_infos, getters, setters, constants)
        self._interfaces_[iid] = new_interface
        self._interface_names_[iid.name] = new_interface
        # No point remembering these.
        del interface_infos[iid]

    def QueryInterface(self, iid):
        if self._interfaces_.has_key(iid):
            assert self._interface_names_.has_key(iid.name), "_interfaces_ has the key, but _interface_names_ does not!"
            return self
        # Haven't seen this before - do a real QI.
        if not self._interface_infos_.has_key(iid):
            self._remember_interface_info(iid)
        ret = self._comobj_.QueryInterface(iid, 0)
#        print "Component QI for", iid, "yielded", ret
        self._make_interface_info(ret, iid)
        assert self._interfaces_.has_key(iid) and self._interface_names_.has_key(iid.name), "Making the interface didn't update the maps"
        return self

    queryInterface = QueryInterface # Alternate name.

    def __getattr__(self, attr):
        if attr in _special_getattr_names:
            raise AttributeError, attr
        # First allow the interface name to return the "raw" interface
        interface = self.__dict__['_interface_names_'].get(attr, None)
        if interface is not None:
            return interface
        interface_name = self.__dict__['_name_to_interface_name_'].get(attr, None)
        # This may be first time trying this interface - get the nsIClassInfo
        if interface_name is None and not self._tried_classinfo_:
            self._build_all_supported_interfaces_()
            interface_name = self.__dict__['_name_to_interface_name_'].get(attr, None)
            
        if interface_name is not None:
            interface = self.__dict__['_interface_names_'].get(interface_name, None)
            if interface is None:
                iid = XPTI_GetInterfaceInfoManager().GetInfoForName(interface_name).GetIID()
                self.QueryInterface(iid)
                interface = self.__dict__['_interface_names_'][interface_name]
            return getattr(interface, attr)
        # Some interfaces may provide this name via "native" support.
        # Loop over all interfaces, and if found, cache it for next time.
        for interface in self.__dict__['_interfaces_'].values():
            try:
                ret = getattr(interface, attr)
                self.__dict__['_name_to_interface_name_'][attr] = interface._iid_.name
                return ret
            except AttributeError:
                pass
        raise AttributeError, "XPCOM component '%s' has no attribute '%s'" % (self._object_name_, attr)
        
    def __setattr__(self, attr, val):
        interface_name = self._name_to_interface_name_.get(attr, None)
        # This may be first time trying this interface - get the nsIClassInfo
        if interface_name is None and not self._tried_classinfo_:
            self._build_all_supported_interfaces_()
            interface_name = self.__dict__['_name_to_interface_name_'].get(attr, None)
        if interface_name is not None:
            interface = self._interface_names_.get(interface_name, None)
            if interface is None:
                iid = XPTI_GetInterfaceInfoManager().GetInfoForName(interface_name).GetIID()
                self.QueryInterface(iid)
                interface = self.__dict__['_interface_names_'][interface_name]
            setattr(interface, attr, val)
            return
        raise AttributeError, "XPCOM component '%s' can not set attribute '%s'" % (self._object_name_, attr)
    def __repr__(self):
        # We can advantage from nsIClassInfo - use it.
        if not self._tried_classinfo_:
            self._build_all_supported_interfaces_()
        assert self._tried_classinfo_, "Should have tried the class info by now!"
        infos = self.__dict__['_interface_infos_']
        if infos:
            iface_names = ",".join([iid.name for iid in infos.keys()])
            iface_desc = " (implementing %s)" % (iface_names,)
        else:
            iface_desc = " (with no class info)"
        return "<XPCOM component '%s'%s>" % (self._object_name_,iface_desc)

class _Interface(_XPCOMBase):
    def __init__(self, comobj, iid, method_infos, getters, setters, constants):
        self.__dict__['_comobj_'] = comobj
        self.__dict__['_iid_'] = iid
        self.__dict__['_property_getters_'] = getters
        self.__dict__['_property_setters_'] = setters
        self.__dict__['_method_infos_'] = method_infos # method infos waiting to be turned into real methods.
        self.__dict__['_methods_'] = {} # unbound methods
        self.__dict__['_object_name_'] = iid.name
        self.__dict__.update(constants)
        # We remember the constant names to prevent the user trying to assign to them!
        self.__dict__['_constant_names_'] = constants.keys()

    def __getattr__(self, attr):
        # Allow the underlying interface to provide a better implementation if desired.
        if attr in _special_getattr_names:
            raise AttributeError, attr

        ret = getattr(self.__dict__['_comobj_'], attr, None)
        if ret is not None:
            return ret
        # Do the function thing first.
        unbound_method = self.__dict__['_methods_'].get(attr, None)
        if unbound_method is not None:
            return new.instancemethod(unbound_method, self, self.__class__)

        getters = self.__dict__['_property_getters_']
        info = getters.get(attr)
        if info is not None:
            method_index, param_infos = info
            if len(param_infos)!=1: # Only expecting a retval
                raise RuntimeError, "Can't get properties with this many args!"
            args = ( param_infos, () )
            return XPTC_InvokeByIndex(self, method_index, args)

        # See if we have a method info waiting to be turned into a method.
        # Do this last as it is a one-off hit.
        method_info = self.__dict__['_method_infos_'].get(attr, None)
        if method_info is not None:
            unbound_method = BuildMethod(method_info, self._iid_)
            # Cache it locally
            self.__dict__['_methods_'][attr] = unbound_method
            return new.instancemethod(unbound_method, self, self.__class__)

        raise AttributeError, "XPCOM component '%s' has no attribute '%s'" % (self._object_name_, attr)

    def __setattr__(self, attr, val):
        # If we already have a __dict__ item of that name, and its not one of
        # our constants, we just directly set it, and leave.
        if self.__dict__.has_key(attr) and attr not in self.__dict__['_constant_names_']:
            self.__dict__[attr] = val
            return
        # Start sniffing for what sort of attribute this might be?
        setters = self.__dict__['_property_setters_']
        info = setters.get(attr)
        if info is None:
            raise AttributeError, "XPCOM component '%s' can not set attribute '%s'" % (self._object_name_, attr)
        method_index, param_infos = info
        if len(param_infos)!=1: # Only expecting a single input val
            raise RuntimeError, "Can't set properties with this many args!"
        real_param_infos = ( param_infos, (val,) )
        return XPTC_InvokeByIndex(self, method_index, real_param_infos)

    def __repr__(self):
        return "<XPCOM interface '%s'>" % (self._object_name_,)


# Called by the _xpcom C++ framework to wrap interfaces up just
# before they are returned.
def MakeInterfaceResult(ob, iid):
    return Component(ob, iid)

class WeakReference:
    """A weak-reference object.  You construct a weak reference by passing
    any COM object you like.  If the object does not support weak
    refs, you will get a standard NS_NOINTERFACE exception.

    Once you have a weak-reference, you can "call" the object to get
    back a strong reference.  Eg:

    >>> some_ob = components.classes['...']
    >>> weak_ref = WeakReference(some_ob)
    >>> new_ob = weak_ref() # new_ob is effectively "some_ob" at this point
    >>> # EXCEPT: new_ob may be None of some_ob has already died - a
    >>> # weak reference does not keep the object alive (that is the point)

    You should never hold onto this resulting strong object for a long time,
    or else you defeat the purpose of the weak-reference.
    """
    def __init__(self, ob, iid = None):
        swr = Component(ob._comobj_, IID_nsISupportsWeakReference)
        self._comobj_ = Component(swr.GetWeakReference()._comobj_, IID_nsIWeakReference)
        if iid is None:
            try:
                iid = ob.IID
            except AttributeError:
                iid = IID_nsISupports
        self._iid_ = iid
    def __call__(self, iid = None):
        if iid is None: iid = self._iid_
        try:
            return Component(self._comobj_.QueryReferent(iid)._comobj_, iid)
        except COMException, details:
            if details.errno != nsError.NS_ERROR_NULL_POINTER:
                raise
            return None
