/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIFrameUtil.h"
#include "nsFrame.h"
#include "nsString.h"
#include "nsRect.h"
#include <stdlib.h>
#include "plstr.h"


#ifdef NS_DEBUG
class nsFrameUtil : public nsIFrameUtil {
public:
  nsFrameUtil();
  virtual ~nsFrameUtil();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CompareRegressionData(FILE* aFile1, FILE* aFile2,PRInt32 aRegressionOutput=0);
  NS_IMETHOD DumpRegressionData(FILE* aInputFile, FILE* aOutputFile);

  struct Node;
  struct Tag;

  struct NodeList {
    NodeList();
    ~NodeList();

    static void Destroy(NodeList* aLists);

    NodeList* next;            // for lists of lists
    Node* node;
    char* name;
  };

  struct Node {
    Node();
    ~Node();

    static void Destroy(Node* aNode);

    static Node* Read(FILE* aFile, Tag* aTag);

    static Node* ReadTree(FILE* aFile);

    Node* next;
    char* type;
    PRUint32 state;
    nsRect bbox;
    nsCString styleData;
    NodeList* lists;
  };

  struct Tag {
    Tag();
    ~Tag();

    static Tag* Parse(FILE* aFile);

    void AddAttr(char* aAttr, char* aValue);

    char* GetAttr(char* aAttr);

    void ReadAttrs(FILE* aFile);

    void ToString(nsString& aResult);

    enum Type {
      open,
      close,
      openClose
    };

    char* name;
    Type type;
    char** attributes;
    PRInt32 num;
    PRInt32 size;
    char** values;
  };

  static char* Copy(char* aString);

  static void DumpNode(Node* aNode, FILE* aOutputFile, PRInt32 aIndent);
  static void DumpTree(Node* aNode, FILE* aOutputFile, PRInt32 aIndent);
  static PRBool CompareTrees(Node* aNode1, Node* aNode2);
};

char*
nsFrameUtil::Copy(char* aString)
{
  if (aString) {
    int l = ::strlen(aString);
    char* c = new char[l+1];
    memcpy(c, aString, l+1);
    return c;
  }
  return aString;
}

//----------------------------------------------------------------------

nsFrameUtil::NodeList::NodeList()
  : next(nsnull), node(nsnull), name(nsnull)
{
}

nsFrameUtil::NodeList::~NodeList()
{
  if (nsnull != name) {
    delete name;
  }
  if (nsnull != node) {
    Node::Destroy(node);
  }
}

void
nsFrameUtil::NodeList::Destroy(NodeList* aLists)
{
  while (nsnull != aLists) {
    NodeList* next = aLists->next;
    delete aLists;
    aLists = next;
  }
}

//----------------------------------------------------------------------

nsFrameUtil::Node::Node()
  : next(nsnull), type(nsnull), state(0), lists(nsnull)
{
}

nsFrameUtil::Node::~Node()
{
  if (nsnull != type) {
    delete type;
  }
  if (nsnull != lists) {
    NodeList::Destroy(lists);
  }
}

void
nsFrameUtil::Node::Destroy(Node* aList)
{
  while (nsnull != aList) {
    Node* next = aList->next;
    delete aList;
    aList = next;
  }
}

static PRInt32 GetInt(nsFrameUtil::Tag* aTag, char* aAttr)
{
  char* value = aTag->GetAttr(aAttr);
  if (nsnull != value) {
    return PRInt32( atoi(value) );
  }
  return 0;
}

nsFrameUtil::Node*
nsFrameUtil::Node::ReadTree(FILE* aFile)
{
  Tag* tag = Tag::Parse(aFile);
  if (nsnull == tag) {
    return nsnull;
  }
  if (PL_strcmp(tag->name, "frame") != 0) {
    delete tag;
    return nsnull;
  }
  Node* result = Read(aFile, tag);
  fclose(aFile);
  return result;
}

nsFrameUtil::Node*
nsFrameUtil::Node::Read(FILE* aFile, Tag* tag)
{
  Node* node = new Node;
  node->type = Copy(tag->GetAttr("type"));
  node->state = GetInt(tag, "state");
  delete tag;

  for (;;) {
    tag = Tag::Parse(aFile);
    if (nsnull == tag) break;
    if (PL_strcmp(tag->name, "frame") == 0) {
      delete tag;
      break;
    }
    if (PL_strcmp(tag->name, "bbox") == 0) {
      nscoord x = nscoord( GetInt(tag, "x") );
      nscoord y = nscoord( GetInt(tag, "y") );
      nscoord w = nscoord( GetInt(tag, "w") );
      nscoord h = nscoord( GetInt(tag, "h") );
      node->bbox.SetRect(x, y, w, h);
    }
    else if (PL_strcmp(tag->name, "child-list") == 0) {
      NodeList* list = new NodeList();
      list->name = Copy(tag->GetAttr("name"));
      list->next = node->lists;
      node->lists = list;
      delete tag;

      Node** tailp = &list->node;
      for (;;) {
        tag = Tag::Parse(aFile);
        if (nsnull == tag) {
          break;
        }
        if (PL_strcmp(tag->name, "child-list") == 0) {
          break;
        }
        if (PL_strcmp(tag->name, "frame") != 0) {
          break;
        }
        Node* child = Node::Read(aFile, tag);
        if (nsnull == child) {
          break;
        }
        *tailp = child;
        tailp = &child->next;
      }
    }
    else if((PL_strcmp(tag->name, "font") == 0) ||
            (PL_strcmp(tag->name, "color") == 0) ||
            (PL_strcmp(tag->name, "spacing") == 0) ||
            (PL_strcmp(tag->name, "list") == 0) ||
            (PL_strcmp(tag->name, "position") == 0) ||
            (PL_strcmp(tag->name, "text") == 0) ||
            (PL_strcmp(tag->name, "display") == 0) ||
            (PL_strcmp(tag->name, "table") == 0) ||
            (PL_strcmp(tag->name, "content") == 0) ||
            (PL_strcmp(tag->name, "UI") == 0) ||
            (PL_strcmp(tag->name, "print") == 0)) {
      char* attr = tag->GetAttr("data");
      node->styleData.Append('|');
      node->styleData.Append((const char *)(attr ? attr : "null attr"));
    }

    delete tag;
  }
  return node;
}

//----------------------------------------------------------------------

nsFrameUtil::Tag::Tag()
  : name(nsnull), type(open), attributes(nsnull), num(0), size(0),
    values(nsnull)
{
}

nsFrameUtil::Tag::~Tag()
{
  PRInt32 i, n = num;
  if (0 != n) {
    for (i = 0; i < n; i++) {
      delete attributes[i];
      delete values[i];
    }
    delete attributes;
    delete values;
  }
}

void
nsFrameUtil::Tag::AddAttr(char* aAttr, char* aValue)
{
  if (num == size) {
    PRInt32 newSize = size * 2 + 4;
    char** a = new char*[newSize];
    char** v = new char*[newSize];
    if (0 != num) {
      memcpy(a, attributes, num * sizeof(char*));
      memcpy(v, values, num * sizeof(char*));
      delete attributes;
      delete values;
    }
    attributes = a;
    values = v;
    size = newSize;
  }
  attributes[num] = aAttr;
  values[num] = aValue;
  num = num + 1;
}

char*
nsFrameUtil::Tag::GetAttr(char* aAttr)
{
  PRInt32 i, n = num;
  for (i = 0; i < n; i++) {
    if (PL_strcmp(attributes[i], aAttr) == 0) {
      return values[i];
    }
  }
  return nsnull;
}

static inline int IsWhiteSpace(int c) {
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

static PRBool EatWS(FILE* aFile)
{
  for (;;) {
    int c = getc(aFile);
    if (c < 0) {
      return PR_FALSE;
    }
    if (!IsWhiteSpace(c)) {
      ungetc(c, aFile);
      break;
    }
  }
  return PR_TRUE;
}

static PRBool Expect(FILE* aFile, char aChar)
{
  int c = getc(aFile);
  if (c < 0) return PR_FALSE;
  if (c != aChar) {
    ungetc(c, aFile);
    return PR_FALSE;
  }
  return PR_TRUE;
}

static char* ReadIdent(FILE* aFile)
{
  char id[1000];
  char* ip = id;
  char* end = ip + sizeof(id) - 1;
  while (ip < end) {
    int c = fgetc(aFile);
    if (c < 0) return nsnull;
    if ((c == '=') || (c == '>') || (c == '/') || IsWhiteSpace(c)) {
      ungetc(c, aFile);
      break;
    }
    *ip++ = char(c);
  }
  *ip = '\0';
  return nsFrameUtil::Copy(id);
}

static char* ReadString(FILE* aFile)
{
  if (!Expect(aFile, '\"')) {
    return nsnull;
  }
  char id[1000];
  char* ip = id;
  char* end = ip + sizeof(id) - 1;
  while (ip < end) {
    int c = fgetc(aFile);
    if (c < 0) return nsnull;
    if (c == '\"') {
      break;
    }
    *ip++ = char(c);
  }
  *ip = '\0';
  return nsFrameUtil::Copy(id);
}

void
nsFrameUtil::Tag::ReadAttrs(FILE* aFile)
{
  for (;;) {
    if (!EatWS(aFile)) {
      break;
    }
    int c = getc(aFile);
    if (c < 0) break;
    if (c == '/') {
      if (!EatWS(aFile)) {
        return;
      }
      if (Expect(aFile, '>')) {
        type = openClose;
        break;
      }
    }
    else if (c == '>') {
      break;
    }
    ungetc(c, aFile);
    char* attr = ReadIdent(aFile);
    if ((nsnull == attr) || !EatWS(aFile)) {
      break;
    }
    char* value = nsnull;
    if (Expect(aFile, '=')) {
      value = ReadString(aFile);
      if (nsnull == value) {
        break;
      }
    }
    AddAttr(attr, value);
  }
}

nsFrameUtil::Tag*
nsFrameUtil::Tag::Parse(FILE* aFile)
{
  if (!EatWS(aFile)) {
    return nsnull;
  }
  if (Expect(aFile, '<')) {
    Tag* tag = new Tag;
    if (Expect(aFile, '/')) {
      tag->type = close;
    }
    else {
      tag->type = open;
    }
    tag->name = ReadIdent(aFile);
    tag->ReadAttrs(aFile);
    return tag;
  }
  return nsnull;
}

void
nsFrameUtil::Tag::ToString(nsString& aResult)
{
  aResult.Truncate();
  aResult.Append(PRUnichar('<'));
  if (type == close) {
    aResult.Append(PRUnichar('/'));
  }
  aResult.AppendWithConversion(name);
  if (0 != num) {
    PRInt32 i, n = num;
    for (i = 0; i < n; i++) {
      aResult.Append(PRUnichar(' '));
      aResult.AppendWithConversion(attributes[i]);
      if (values[i]) {
        aResult.Append(NS_LITERAL_STRING("=\""));
        aResult.AppendWithConversion(values[i]);
        aResult.Append(PRUnichar('\"'));
      }
    }
  }
  if (type == openClose) {
    aResult.Append(PRUnichar('/'));
  }
  aResult.Append(PRUnichar('>'));
}

//----------------------------------------------------------------------

nsresult NS_NewFrameUtil(nsIFrameUtil** aResult);
nsresult
NS_NewFrameUtil(nsIFrameUtil** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null pointer");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;

  nsFrameUtil* it = new nsFrameUtil();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIFrameUtil), (void**) aResult);
}

nsFrameUtil::nsFrameUtil()
{
  NS_INIT_REFCNT();
}

nsFrameUtil::~nsFrameUtil()
{
}

NS_IMPL_ISUPPORTS1(nsFrameUtil, nsIFrameUtil)

void
nsFrameUtil::DumpNode(Node* aNode, FILE* aOutputFile, PRInt32 aIndent)
{
  nsFrame::IndentBy(aOutputFile, aIndent);
  fprintf(aOutputFile, "%s 0x%x %d,%d,%d,%d, %s\n", aNode->type, aNode->state,
          aNode->bbox.x, aNode->bbox.y,
          aNode->bbox.width, aNode->bbox.height,
          aNode->styleData.get());
}

void
nsFrameUtil::DumpTree(Node* aNode, FILE* aOutputFile, PRInt32 aIndent)
{
  while (nsnull != aNode) {
    DumpNode(aNode, aOutputFile, aIndent);
    nsFrameUtil::NodeList* lists = aNode->lists;
    if (nsnull != lists) {
      while (nsnull != lists) {
        nsFrame::IndentBy(aOutputFile, aIndent);
        fprintf(aOutputFile, " list: %s\n",
                lists->name ? lists->name : "primary");
        DumpTree(lists->node, aOutputFile, aIndent + 1);
        lists = lists->next;
      }
    }
    aNode = aNode->next;
  }
}

PRBool
nsFrameUtil::CompareTrees(Node* tree1, Node* tree2)
{
  PRBool result = PR_TRUE;
  for (;; tree1 = tree1->next, tree2 = tree2->next) {
    // Make sure both nodes are non-null, or at least agree with each other
    if (nsnull == tree1) {
      if (nsnull == tree2) {
        break;
      }
      printf("first tree prematurely ends\n");
      return PR_FALSE;
    }
    else if (nsnull == tree2) {
      printf("second tree prematurely ends\n");
      return PR_FALSE;
    }

    // Check the attributes that we care about
    if (0 != PL_strcmp(tree1->type, tree2->type)) {
      printf("frame type mismatch: %s vs. %s\n", tree1->type, tree2->type);
      printf("Node 1:\n");
      DumpNode(tree1, stdout, 1);
      printf("Node 2:\n");
      DumpNode(tree2, stdout, 1);
      return PR_FALSE;
    }

    // Ignore the XUL scrollbar frames
    static const char kScrollbarFrame[] = "ScrollbarFrame";
    if (0 == PL_strncmp(tree1->type, kScrollbarFrame, sizeof(kScrollbarFrame) - 1))
      continue;

    // We'll ignore these flags for the purposes of comparing frame state:
    //
    //   NS_FRAME_EXTERNAL_REFERENCE
    //     because this is set by the event state manager or the
    //     caret code when a frame is focused. Depending on whether
    //     or not the regression tests are run as the focused window
    //     will make this value vary randomly.
#define IRRELEVANT_FRAME_STATE_FLAGS NS_FRAME_EXTERNAL_REFERENCE

#define FRAME_STATE_MASK (~(IRRELEVANT_FRAME_STATE_FLAGS))

    if ((tree1->state & FRAME_STATE_MASK) != (tree2->state & FRAME_STATE_MASK)) {
      printf("frame state mismatch: 0x%x vs. 0x%x\n",
             tree1->state, tree2->state);
      printf("Node 1:\n");
      DumpNode(tree1, stdout, 1);
      printf("Node 2:\n");
      DumpNode(tree2, stdout, 1);
      result = PR_FALSE; // we have a non-critical failure, so remember that but continue
    }
    if (tree1->bbox != tree2->bbox) {
      printf("frame bbox mismatch: %d,%d,%d,%d vs. %d,%d,%d,%d\n",
             tree1->bbox.x, tree1->bbox.y,
             tree1->bbox.width, tree1->bbox.height,
             tree2->bbox.x, tree2->bbox.y,
             tree2->bbox.width, tree2->bbox.height);
      printf("Node 1:\n");
      DumpNode(tree1, stdout, 1);
      printf("Node 2:\n");
      DumpNode(tree2, stdout, 1);
      result = PR_FALSE; // we have a non-critical failure, so remember that but continue
    }
    if (tree1->styleData != tree2->styleData) {
      printf("frame style data mismatch: %s vs. %s\n",
        tree1->styleData.get(),
        tree2->styleData.get());
    }

    // Check child lists too
    NodeList* list1 = tree1->lists;
    NodeList* list2 = tree2->lists;
    for (;;) {
      if (nsnull == list1) {
        if (nsnull != list2) {
          printf("first tree prematurely ends (no child lists)\n");
          printf("Node 1:\n");
          DumpNode(tree1, stdout, 1);
          printf("Node 2:\n");
          DumpNode(tree2, stdout, 1);
          return PR_FALSE;
        }
        else {
          break;
        }
      }
      if (nsnull == list2) {
        printf("second tree prematurely ends (no child lists)\n");
        printf("Node 1:\n");
        DumpNode(tree1, stdout, 1);
        printf("Node 2:\n");
        DumpNode(tree2, stdout, 1);
        return PR_FALSE;
      }
      if (0 != PL_strcmp(list1->name, list2->name)) {
        printf("child-list name mismatch: %s vs. %s\n",
               list1->name ? list1->name : "(null)",
               list2->name ? list2->name : "(null)");
        result = PR_FALSE; // we have a non-critical failure, so remember that but continue
      }
      else {
        PRBool equiv = CompareTrees(list1->node, list2->node);
        if (!equiv) {
          return equiv;
        }
      }
      list1 = list1->next;
      list2 = list2->next;
    }
  }
  return result;
}

NS_IMETHODIMP
nsFrameUtil::CompareRegressionData(FILE* aFile1, FILE* aFile2,PRInt32 aRegressionOutput)
{
  Node* tree1 = Node::ReadTree(aFile1);
  Node* tree2 = Node::ReadTree(aFile2);

  nsresult rv = NS_OK;
  if (!CompareTrees(tree1, tree2)) {
    // only output this if aRegressionOutput is 0
    if( 0 == aRegressionOutput ){
      printf("Regression data 1:\n");
      DumpTree(tree1, stdout, 0);
      printf("Regression data 2:\n");
      DumpTree(tree2, stdout, 0);
    }
    rv = NS_ERROR_FAILURE;
  }

  Node::Destroy(tree1);
  Node::Destroy(tree2);

  return rv;
}

NS_IMETHODIMP
nsFrameUtil::DumpRegressionData(FILE* aInputFile, FILE* aOutputFile)
{
  Node* tree1 = Node::ReadTree(aInputFile);
  if (nsnull != tree1) {
    DumpTree(tree1, aOutputFile, 0);
    Node::Destroy(tree1);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
#endif
