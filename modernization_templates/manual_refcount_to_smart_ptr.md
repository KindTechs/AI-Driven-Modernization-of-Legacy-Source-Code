# Manual Reference Counting to Smart Pointer Modernization Template

## Overview
This template demonstrates how to convert Mozilla 1.0 manual reference counting patterns (AddRef/Release) to modern smart pointer patterns (nsCOMPtr, RefPtr), eliminating memory leaks and improving code safety.

## Pattern Description
Mozilla 1.0 code frequently uses manual reference counting with AddRef/Release calls, which is error-prone and can lead to memory leaks, use-after-free bugs, and double-free crashes. Smart pointers provide automatic reference counting with RAII semantics.

## Before (Legacy Pattern)
```cpp
class nsSelection {
private:
  nsIDOMRange* mCurrentRange;  // Raw pointer requiring manual refcounting
  nsIPresShell* mPresShell;    // Raw pointer requiring manual refcounting
  
public:
  nsSelection() : mCurrentRange(nullptr), mPresShell(nullptr) {}
  
  ~nsSelection() {
    // Manual cleanup - easy to forget or get wrong
    if (mCurrentRange) {
      mCurrentRange->Release();
      mCurrentRange = nullptr;
    }
    if (mPresShell) {
      mPresShell->Release();
      mPresShell = nullptr;
    }
  }
  
  nsresult SetCurrentRange(nsIDOMRange* aRange) {
    // Manual reference counting - error-prone
    if (mCurrentRange) {
      mCurrentRange->Release();
    }
    mCurrentRange = aRange;
    if (mCurrentRange) {
      mCurrentRange->AddRef();
    }
    return NS_OK;
  }
  
  nsresult GetCurrentRange(nsIDOMRange** aRange) {
    if (!aRange) return NS_ERROR_NULL_POINTER;
    *aRange = mCurrentRange;
    if (*aRange) {
      (*aRange)->AddRef();  // Caller must Release
    }
    return NS_OK;
  }
  
  nsresult CopyRange(nsIDOMRange* aSource, nsIDOMRange** aDestination) {
    if (!aSource || !aDestination) return NS_ERROR_NULL_POINTER;
    
    // Manual reference counting throughout
    aSource->AddRef();
    *aDestination = aSource;
    
    return NS_OK;
  }
  
  // Complex method with multiple reference counted objects
  nsresult ProcessRanges() {
    nsIDOMRange* range1 = nullptr;
    nsIDOMRange* range2 = nullptr;
    nsIPresShell* presShell = nullptr;
    
    nsresult rv = GetCurrentRange(&range1);
    if (NS_FAILED(rv)) return rv;
    
    rv = GetSecondRange(&range2);
    if (NS_FAILED(rv)) {
      if (range1) range1->Release();  // Manual cleanup
      return rv;
    }
    
    rv = GetPresShell(&presShell);
    if (NS_FAILED(rv)) {
      if (range1) range1->Release();  // Manual cleanup
      if (range2) range2->Release();  // Manual cleanup
      return rv;
    }
    
    // Use objects...
    
    // Manual cleanup - error-prone and verbose
    if (range1) range1->Release();
    if (range2) range2->Release();
    if (presShell) presShell->Release();
    
    return NS_OK;
  }
};

// Usage - caller must remember to Release
nsIDOMRange* range = nullptr;
nsresult rv = selection->GetCurrentRange(&range);
if (NS_SUCCEEDED(rv) && range) {
  // Use range...
  range->Release();  // Easy to forget!
}
```

## After (Modern Pattern)
```cpp
class nsSelection {
private:
  nsCOMPtr<nsIDOMRange> mCurrentRange;  // Smart pointer - automatic refcounting
  nsCOMPtr<nsIPresShell> mPresShell;    // Smart pointer - automatic refcounting
  
public:
  nsSelection() = default;  // Smart pointers initialize to nullptr
  
  ~nsSelection() = default;  // Smart pointers clean up automatically
  
  nsresult SetCurrentRange(nsIDOMRange* aRange) {
    mCurrentRange = aRange;  // Automatic AddRef/Release
    return NS_OK;
  }
  
  // Modern getter returning smart pointer
  nsCOMPtr<nsIDOMRange> GetCurrentRangeModern() {
    return mCurrentRange;
  }
  
  // Compatibility wrapper for existing code
  nsresult GetCurrentRange(nsIDOMRange** aRange) {
    if (!aRange) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMRange> range = mCurrentRange;
    range.forget(aRange);  // Transfer ownership
    return NS_OK;
  }
  
  nsresult CopyRange(nsIDOMRange* aSource, nsIDOMRange** aDestination) {
    if (!aSource || !aDestination) return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIDOMRange> source = aSource;  // Automatic AddRef
    source.forget(aDestination);             // Transfer to caller
    
    return NS_OK;
  }
  
  // Modern method - clean and safe
  nsresult ProcessRanges() {
    nsCOMPtr<nsIDOMRange> range1 = GetCurrentRangeModern();
    if (!range1) return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsIDOMRange> range2 = GetSecondRangeModern();
    if (!range2) return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsIPresShell> presShell = GetPresShellModern();
    if (!presShell) return NS_ERROR_NOT_AVAILABLE;
    
    // Use objects...
    
    // Automatic cleanup when objects go out of scope
    return NS_OK;
  }
};

// Modern usage - automatic cleanup
nsCOMPtr<nsIDOMRange> range = selection->GetCurrentRangeModern();
if (range) {
  // Use range...
  // Automatic cleanup when range goes out of scope
}
```

## Step-by-Step Implementation

### Step 1: Identify Manual Reference Counting Patterns
```cpp
// Look for these patterns:
obj->AddRef();
obj->Release();
if (obj) obj->Release();
NS_ADDREF(obj);
NS_RELEASE(obj);
NS_IF_RELEASE(obj);
```

### Step 2: Choose Appropriate Smart Pointer Type
```cpp
// For XPCOM interfaces
nsIDOMRange* raw;           // -> nsCOMPtr<nsIDOMRange>
nsIPresShell* raw;          // -> nsCOMPtr<nsIPresShell>

// For Mozilla reference-counted objects
nsINode* raw;               // -> RefPtr<nsINode>
nsIContent* raw;            // -> RefPtr<nsIContent>

// For objects with custom reference counting
MyRefCountedClass* raw;     // -> RefPtr<MyRefCountedClass>
```

### Step 3: Update Member Variables
```cpp
// Before
class MyClass {
private:
  nsIDOMRange* mRange;
  nsIPresShell* mPresShell;
  
public:
  MyClass() : mRange(nullptr), mPresShell(nullptr) {}
  
  ~MyClass() {
    if (mRange) mRange->Release();
    if (mPresShell) mPresShell->Release();
  }
};

// After
class MyClass {
private:
  nsCOMPtr<nsIDOMRange> mRange;
  nsCOMPtr<nsIPresShell> mPresShell;
  
public:
  MyClass() = default;   // Smart pointers initialize to nullptr
  ~MyClass() = default;  // Automatic cleanup
};
```

### Step 4: Update Assignment Operations
```cpp
// Before - Manual reference counting
void SetRange(nsIDOMRange* aRange) {
  if (mRange) {
    mRange->Release();
  }
  mRange = aRange;
  if (mRange) {
    mRange->AddRef();
  }
}

// After - Automatic reference counting
void SetRange(nsIDOMRange* aRange) {
  mRange = aRange;  // nsCOMPtr handles AddRef/Release
}

// Even better - accept smart pointer
void SetRange(const nsCOMPtr<nsIDOMRange>& aRange) {
  mRange = aRange;
}

// Or with move semantics
void SetRange(nsCOMPtr<nsIDOMRange>&& aRange) {
  mRange = std::move(aRange);
}
```

### Step 5: Update Getter Methods
```cpp
// Before - Manual AddRef for caller
nsIDOMRange* GetRange() {
  if (mRange) {
    mRange->AddRef();
  }
  return mRange;
}

// After - Modern getter
nsCOMPtr<nsIDOMRange> GetRange() {
  return mRange;
}

// Compatibility wrapper
nsresult GetRange(nsIDOMRange** aRange) {
  if (!aRange) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMRange> range = mRange;
  range.forget(aRange);
  return NS_OK;
}
```

### Step 6: Update Method Parameters
```cpp
// Before - Raw pointers requiring manual counting
nsresult ProcessRange(nsIDOMRange* aRange) {
  if (aRange) {
    aRange->AddRef();
  }
  // Use aRange...
  if (aRange) {
    aRange->Release();
  }
  return NS_OK;
}

// After - Smart pointer parameters
nsresult ProcessRange(const nsCOMPtr<nsIDOMRange>& aRange) {
  // Use aRange...
  // Automatic cleanup
  return NS_OK;
}

// Or for temporary usage
nsresult ProcessRange(nsIDOMRange* aRange) {
  nsCOMPtr<nsIDOMRange> range = aRange;  // Automatic AddRef
  // Use range...
  // Automatic cleanup
  return NS_OK;
}
```

### Step 7: Update Local Variables
```cpp
// Before - Manual reference counting
nsresult SomeMethod() {
  nsIDOMRange* range = nullptr;
  nsresult rv = GetRange(&range);
  if (NS_FAILED(rv)) return rv;
  
  nsIPresShell* presShell = nullptr;
  rv = GetPresShell(&presShell);
  if (NS_FAILED(rv)) {
    if (range) range->Release();  // Manual cleanup
    return rv;
  }
  
  // Use objects...
  
  // Manual cleanup
  if (range) range->Release();
  if (presShell) presShell->Release();
  
  return NS_OK;
}

// After - Automatic reference counting
nsresult SomeMethod() {
  nsCOMPtr<nsIDOMRange> range;
  nsresult rv = GetRange(getter_AddRefs(range));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIPresShell> presShell;
  rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;  // range cleans up automatically
  
  // Use objects...
  
  // Automatic cleanup when objects go out of scope
  return NS_OK;
}
```

## Smart Pointer Utilities

### nsCOMPtr Utilities
```cpp
// Assignment from raw pointer
nsCOMPtr<nsIDOMRange> range = rawRange;  // AddRef called automatically

// Getting raw pointer
nsIDOMRange* raw = range.get();  // No AddRef, temporary use only

// Transferring ownership
nsIDOMRange* transferred = range.forget();  // Caller must Release

// For out parameters
GetRange(getter_AddRefs(range));

// Null checking
if (range) {
  // range is valid
}

// Comparison
if (range1 == range2) {
  // Same object
}
```

### RefPtr Utilities
```cpp
// Similar to nsCOMPtr but for different object types
RefPtr<nsINode> node = rawNode;
RefPtr<nsIContent> content = node->AsContent();

// Move semantics
RefPtr<nsINode> moved = std::move(node);  // node is now null

// Forget for out parameters
rawNode = node.forget();
```

## Compatibility Considerations

### Existing Code Integration
- **Gradual migration**: Can mix smart pointers with raw pointers
- **Wrapper methods**: Provide compatibility for existing APIs
- **Performance**: Smart pointers have minimal overhead

### Build System Integration
- **Headers**: Include appropriate smart pointer headers
- **Linking**: No additional libraries required
- **Compiler**: Works with older C++ standards

## Benefits of Modernization

### Memory Safety
- **Automatic cleanup**: No manual AddRef/Release
- **Exception safety**: RAII ensures cleanup even with exceptions
- **Leak prevention**: Impossible to forget to Release

### Code Quality
- **Reduced boilerplate**: No manual reference counting code
- **Cleaner destructors**: Often can use = default
- **Self-documenting**: Smart pointer type indicates ownership

### Maintainability
- **Fewer bugs**: Eliminates reference counting errors
- **Easier refactoring**: Less manual memory management
- **Better testing**: Fewer edge cases to test

## Common Patterns and Solutions

### Pattern 1: Conditional Assignment
```cpp
// Before
if (condition) {
  if (mRange) mRange->Release();
  mRange = newRange;
  if (mRange) mRange->AddRef();
}

// After
if (condition) {
  mRange = newRange;  // Automatic handling
}
```

### Pattern 2: Temporary Objects
```cpp
// Before
nsIDOMRange* temp = CreateRange();
if (temp) {
  ProcessRange(temp);
  temp->Release();
}

// After
nsCOMPtr<nsIDOMRange> temp = CreateRange();
if (temp) {
  ProcessRange(temp);
  // Automatic cleanup
}
```

### Pattern 3: Array of Pointers
```cpp
// Before
nsIDOMRange** mRanges;
PRInt32 mRangeCount;

// After
nsTArray<nsCOMPtr<nsIDOMRange>> mRanges;
```

### Pattern 4: Optional Pointers
```cpp
// Before
nsIDOMRange* mOptionalRange;  // Can be nullptr

// After
nsCOMPtr<nsIDOMRange> mOptionalRange;  // Can still be nullptr
```

## Testing Strategy

### Unit Tests
```cpp
TEST(SmartPointerMigration, BasicOperations) {
  nsCOMPtr<nsIDOMRange> range = CreateTestRange();
  ASSERT_TRUE(range);
  
  // Test assignment
  nsCOMPtr<nsIDOMRange> copy = range;
  ASSERT_EQ(range, copy);
  
  // Test reset
  range = nullptr;
  ASSERT_FALSE(range);
  ASSERT_TRUE(copy);  // Still valid
}

TEST(SmartPointerMigration, MemoryManagement) {
  // Test that objects are properly cleaned up
  {
    nsCOMPtr<nsIDOMRange> range = CreateTestRange();
    // range should be cleaned up when going out of scope
  }
  // Verify cleanup with memory testing tools
}
```

### Integration Tests
```cpp
TEST(SmartPointerMigration, BackwardCompatibility) {
  nsSelection selection;
  
  // Test that old API still works
  nsIDOMRange* range = nullptr;
  nsresult rv = selection.GetCurrentRange(&range);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  
  // Test modern API
  nsCOMPtr<nsIDOMRange> modernRange = selection.GetCurrentRangeModern();
  ASSERT_EQ(range, modernRange.get());
  
  // Clean up old API result
  if (range) range->Release();
}
```

## Migration Strategies

### Strategy 1: Bottom-Up
1. Start with utility classes
2. Migrate member variables
3. Update methods one by one
4. Maintain compatibility wrappers

### Strategy 2: Interface-First
1. Update public interfaces
2. Keep internal implementation unchanged initially
3. Gradually migrate internal code
4. Remove compatibility code last

### Strategy 3: File-by-File
1. Choose complete files for migration
2. Update entire file at once
3. Test thoroughly
4. Move to next file

## Migration Checklist

- [ ] Identify all manual AddRef/Release patterns
- [ ] Choose appropriate smart pointer types
- [ ] Update member variables
- [ ] Update constructors and destructors
- [ ] Convert setter methods
- [ ] Convert getter methods
- [ ] Update method parameters
- [ ] Update local variables
- [ ] Create compatibility wrappers
- [ ] Update unit tests
- [ ] Test memory usage
- [ ] Update documentation

## Common Pitfalls

### Pitfall 1: Circular References
```cpp
// Problem: Creates circular reference
class Parent {
  nsCOMPtr<Child> mChild;
};
class Child {
  nsCOMPtr<Parent> mParent;  // Circular!
};

// Solution: Use weak references
class Child {
  nsWeakPtr mParent;  // Weak reference
};
```

### Pitfall 2: Mixing Smart and Raw Pointers
```cpp
// Dangerous: Mixing ownership models
nsCOMPtr<nsIDOMRange> smart = CreateRange();
nsIDOMRange* raw = smart.get();
// Don't call raw->Release() - smart owns it!
```

### Pitfall 3: Performance Issues
```cpp
// Inefficient: Unnecessary AddRef/Release
for (int i = 0; i < 1000; ++i) {
  nsCOMPtr<nsIDOMRange> range = GetRange();  // AddRef
  // Use range briefly
  // Release when going out of scope
}

// Better: Reuse smart pointer
nsCOMPtr<nsIDOMRange> range;
for (int i = 0; i < 1000; ++i) {
  range = GetRange();  // Reuse same smart pointer
  // Use range
}
```

## Performance Considerations

### Smart Pointer Overhead
- **AddRef/Release**: Same cost as manual calls
- **Assignment**: Slightly more expensive due to automatic calls
- **Copying**: More expensive than raw pointer copying
- **Null checking**: Same cost as raw pointer checking

### Optimization Strategies
- **Move semantics**: Use std::move for temporary objects
- **Const references**: Pass smart pointers by const reference
- **Caching**: Cache frequently accessed smart pointers
- **Batch operations**: Group operations to minimize ref count changes

## References

- [nsCOMPtr User Guide](https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide/nsCOMPtr)
- [RefPtr Documentation](https://developer.mozilla.org/en-US/docs/Mozilla/C++_Guide/RefPtr)
- [XPCOM Reference Counting](https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide/Reference_counting)
- [Smart Pointers in C++](https://en.cppreference.com/w/cpp/memory)
- [RAII and Resource Management](https://en.cppreference.com/w/cpp/language/raii)