# Raw Pointer to Smart Pointer Modernization Template

## Overview
This template demonstrates how to convert Mozilla 1.0 raw pointer usage to modern smart pointers (nsCOMPtr, RefPtr, std::unique_ptr, std::shared_ptr), improving memory safety and reducing the risk of memory leaks.

## Pattern Description
Mozilla 1.0 code frequently uses raw pointers with manual memory management, leading to potential memory leaks, use-after-free bugs, and double-free vulnerabilities. Smart pointers provide automatic memory management and clear ownership semantics.

## Before (Legacy Pattern)
```cpp
class nsSelection {
private:
  nsIDOMRange* mCurrentRange;  // Raw pointer
  nsIPresShell* mPresShell;    // Raw pointer
  
public:
  nsSelection() : mCurrentRange(nullptr), mPresShell(nullptr) {}
  
  ~nsSelection() {
    // Manual cleanup - easy to forget or get wrong
    if (mCurrentRange) {
      mCurrentRange->Release();
    }
    if (mPresShell) {
      mPresShell->Release();
    }
  }
  
  nsresult SetCurrentRange(nsIDOMRange* aRange) {
    if (mCurrentRange) {
      mCurrentRange->Release();  // Manual release
    }
    mCurrentRange = aRange;
    if (mCurrentRange) {
      mCurrentRange->AddRef();   // Manual AddRef
    }
    return NS_OK;
  }
  
  nsIDOMRange* GetCurrentRange() {
    if (mCurrentRange) {
      mCurrentRange->AddRef();   // Caller must Release
    }
    return mCurrentRange;
  }
};

// Usage - error-prone
nsIDOMRange* range = selection->GetCurrentRange();
if (range) {
  // Use range...
  range->Release();  // Easy to forget!
}
```

## After (Modern Pattern)
```cpp
class nsSelection {
private:
  nsCOMPtr<nsIDOMRange> mCurrentRange;  // Smart pointer
  nsCOMPtr<nsIPresShell> mPresShell;    // Smart pointer
  
public:
  nsSelection() = default;  // Smart pointers initialize to nullptr
  
  ~nsSelection() = default;  // Smart pointers clean up automatically
  
  nsresult SetCurrentRange(nsIDOMRange* aRange) {
    mCurrentRange = aRange;  // Automatic AddRef/Release
    return NS_OK;
  }
  
  // Modern accessor returning smart pointer
  nsCOMPtr<nsIDOMRange> GetCurrentRangeModern() {
    return mCurrentRange;
  }
  
  // Compatibility wrapper for existing code
  nsIDOMRange* GetCurrentRange() {
    nsCOMPtr<nsIDOMRange> range = mCurrentRange;
    return range.forget();  // Transfer ownership to caller
  }
};

// Modern usage - safe and clean
nsCOMPtr<nsIDOMRange> range = selection->GetCurrentRangeModern();
if (range) {
  // Use range...
  // Automatic cleanup when range goes out of scope
}
```

## Step-by-Step Implementation

### Step 1: Identify Pointer Types and Choose Smart Pointer
```cpp
// COM/XPCOM interfaces -> nsCOMPtr
nsIDOMRange* rawRange;           // Convert to: nsCOMPtr<nsIDOMRange>
nsIPresShell* rawPresShell;      // Convert to: nsCOMPtr<nsIPresShell>

// Reference counted objects -> RefPtr
nsINode* rawNode;                // Convert to: RefPtr<nsINode>
nsIContent* rawContent;          // Convert to: RefPtr<nsIContent>

// Regular C++ objects -> std::unique_ptr or std::shared_ptr
MyClass* rawObject;              // Convert to: std::unique_ptr<MyClass>
SharedResource* rawShared;       // Convert to: std::shared_ptr<SharedResource>
```

### Step 2: Update Member Variables
```cpp
// Before
class nsSelection {
private:
  nsIDOMRange* mCurrentRange;
  nsIPresShell* mPresShell;
  MyClass* mHelper;
  
public:
  nsSelection() : mCurrentRange(nullptr), mPresShell(nullptr), mHelper(nullptr) {}
  
  ~nsSelection() {
    if (mCurrentRange) mCurrentRange->Release();
    if (mPresShell) mPresShell->Release();
    delete mHelper;
  }
};

// After
class nsSelection {
private:
  nsCOMPtr<nsIDOMRange> mCurrentRange;
  nsCOMPtr<nsIPresShell> mPresShell;
  std::unique_ptr<MyClass> mHelper;
  
public:
  nsSelection() = default;        // Smart pointers initialize to nullptr
  ~nsSelection() = default;       // Automatic cleanup
};
```

### Step 3: Update Setter Methods
```cpp
// Before - Manual reference counting
nsresult SetCurrentRange(nsIDOMRange* aRange) {
  if (mCurrentRange) {
    mCurrentRange->Release();
  }
  mCurrentRange = aRange;
  if (mCurrentRange) {
    mCurrentRange->AddRef();
  }
  return NS_OK;
}

// After - Automatic reference counting
nsresult SetCurrentRange(nsIDOMRange* aRange) {
  mCurrentRange = aRange;  // nsCOMPtr handles AddRef/Release automatically
  return NS_OK;
}

// Even better - accept smart pointer directly
nsresult SetCurrentRange(nsCOMPtr<nsIDOMRange> aRange) {
  mCurrentRange = std::move(aRange);
  return NS_OK;
}
```

### Step 4: Update Getter Methods
```cpp
// Before - Manual AddRef for caller
nsIDOMRange* GetCurrentRange() {
  if (mCurrentRange) {
    mCurrentRange->AddRef();
  }
  return mCurrentRange;
}

// After - Modern getter returning smart pointer
nsCOMPtr<nsIDOMRange> GetCurrentRangeModern() {
  return mCurrentRange;
}

// Compatibility wrapper
nsIDOMRange* GetCurrentRange() {
  nsCOMPtr<nsIDOMRange> range = mCurrentRange;
  return range.forget();  // Transfer ownership
}
```

### Step 5: Update Local Variables
```cpp
// Before - Manual memory management
nsresult SomeMethod() {
  nsIDOMRange* range = nullptr;
  nsresult rv = GetRangeAt(0, &range);
  if (NS_FAILED(rv)) return rv;
  
  // Use range...
  
  if (range) {
    range->Release();  // Don't forget!
  }
  return NS_OK;
}

// After - Automatic memory management
nsresult SomeMethod() {
  nsCOMPtr<nsIDOMRange> range;
  nsresult rv = GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(rv)) return rv;
  
  // Use range...
  
  // Automatic cleanup when range goes out of scope
  return NS_OK;
}

// Modern version using Result type
Result<void, nsresult> SomeMethodModern() {
  auto rangeResult = GetRangeAtModern(0);
  if (rangeResult.isErr()) {
    return Err(rangeResult.unwrapErr());
  }
  
  nsCOMPtr<nsIDOMRange> range = rangeResult.unwrap();
  // Use range...
  
  return Ok();
}
```

## Compatibility Considerations

### Existing Code Compatibility
- **Maintain raw pointer APIs**: Keep original methods for backward compatibility
- **Gradual migration**: Allow mixed usage during transition
- **Testing**: Ensure identical behavior between old and new APIs

### Performance Considerations
- **AddRef/Release overhead**: Smart pointers have minimal overhead
- **Move semantics**: Use `std::move` for expensive operations
- **Stack allocation**: Prefer stack-allocated smart pointers

### Build System Integration
- **Header dependencies**: Ensure smart pointer headers are available
- **Linking**: No additional linking required for most smart pointers
- **Compiler support**: Requires C++11 or later

## Benefits of Modernization

### Memory Safety
- **Automatic cleanup**: No manual memory management
- **Exception safety**: RAII ensures cleanup even with exceptions
- **Clear ownership**: Smart pointers express ownership semantics

### Code Quality
- **Reduced boilerplate**: No manual AddRef/Release calls
- **Fewer bugs**: Eliminates memory leaks and use-after-free
- **Self-documenting**: Smart pointer type indicates ownership

### Maintainability
- **Simpler destructors**: Often can use = default
- **Easier refactoring**: Less manual memory management to track
- **Better testing**: Fewer edge cases related to memory management

## Smart Pointer Selection Guide

### nsCOMPtr<T>
**Use for**: XPCOM/COM interfaces
```cpp
nsCOMPtr<nsIDOMRange> range;
nsCOMPtr<nsIPresShell> presShell;
```

### RefPtr<T>
**Use for**: Mozilla reference-counted objects
```cpp
RefPtr<nsINode> node;
RefPtr<nsIContent> content;
```

### std::unique_ptr<T>
**Use for**: Exclusive ownership of C++ objects
```cpp
std::unique_ptr<MyClass> helper;
std::unique_ptr<int[]> buffer;  // Arrays
```

### std::shared_ptr<T>
**Use for**: Shared ownership of C++ objects
```cpp
std::shared_ptr<ExpensiveResource> resource;
```

### std::weak_ptr<T>
**Use for**: Breaking circular references
```cpp
std::weak_ptr<Parent> parent;  // Avoids parent->child->parent cycle
```

## Common Patterns and Solutions

### Pattern 1: Array of Pointers
```cpp
// Before
nsIDOMRange** mRanges;
PRInt32 mRangeCount;

// After
nsTArray<nsCOMPtr<nsIDOMRange>> mRanges;
```

### Pattern 2: Optional Pointers
```cpp
// Before
nsIDOMRange* mOptionalRange;  // May be nullptr

// After
nsCOMPtr<nsIDOMRange> mOptionalRange;  // Can still be nullptr
// Or even better:
Maybe<nsCOMPtr<nsIDOMRange>> mOptionalRange;
```

### Pattern 3: Callback Pointers
```cpp
// Before
nsICallback* mCallback;

// After
nsCOMPtr<nsICallback> mCallback;
```

### Pattern 4: Temporary Objects
```cpp
// Before
nsIDOMRange* CreateTempRange() {
  nsIDOMRange* range = new nsRange();
  range->AddRef();
  return range;  // Caller must Release
}

// After
nsCOMPtr<nsIDOMRange> CreateTempRange() {
  nsCOMPtr<nsIDOMRange> range = new nsRange();
  return range;  // Automatic reference counting
}
```

## Migration Strategies

### Strategy 1: Bottom-Up Migration
1. Start with leaf classes (no dependencies)
2. Migrate member variables first
3. Update methods one by one
4. Maintain compatibility wrappers

### Strategy 2: Interface-First Migration
1. Update public interfaces to return smart pointers
2. Keep internal implementation as raw pointers initially
3. Gradually migrate internal implementation
4. Remove compatibility wrappers last

### Strategy 3: Module-by-Module Migration
1. Choose a complete module for migration
2. Update all classes in the module simultaneously
3. Maintain module interfaces for compatibility
4. Test thoroughly before moving to next module

## Testing Strategy

### Unit Tests
```cpp
TEST(nsSelection, SmartPointerBasics) {
  nsSelection selection;
  
  // Test setting and getting with smart pointers
  nsCOMPtr<nsIDOMRange> range = new nsRange();
  selection.SetCurrentRange(range);
  
  nsCOMPtr<nsIDOMRange> retrieved = selection.GetCurrentRangeModern();
  ASSERT_EQ(range, retrieved);
}

TEST(nsSelection, BackwardCompatibility) {
  nsSelection selection;
  
  // Test that old API still works
  nsIDOMRange* range = new nsRange();
  range->AddRef();
  selection.SetCurrentRange(range);
  
  nsIDOMRange* retrieved = selection.GetCurrentRange();
  ASSERT_EQ(range, retrieved);
  
  // Clean up
  range->Release();
  retrieved->Release();
}
```

### Memory Tests
```cpp
TEST(nsSelection, NoMemoryLeaks) {
  // Use memory testing tools to verify no leaks
  for (int i = 0; i < 1000; ++i) {
    nsSelection selection;
    nsCOMPtr<nsIDOMRange> range = new nsRange();
    selection.SetCurrentRange(range);
    // selection destructor should clean up automatically
  }
}
```

## Migration Checklist

- [ ] Identify all raw pointer member variables
- [ ] Choose appropriate smart pointer types
- [ ] Update constructors and destructors
- [ ] Convert setter methods
- [ ] Convert getter methods
- [ ] Update local variables in methods
- [ ] Create compatibility wrappers
- [ ] Update unit tests
- [ ] Test memory usage and performance
- [ ] Update documentation
- [ ] Plan gradual migration of callers

## Common Pitfalls and Solutions

### Pitfall 1: Circular References
```cpp
// Problem: Parent and child hold strong references to each other
class Parent {
  RefPtr<Child> mChild;
};
class Child {
  RefPtr<Parent> mParent;  // Circular reference!
};

// Solution: Use weak reference in one direction
class Child {
  std::weak_ptr<Parent> mParent;  // Breaks the cycle
};
```

### Pitfall 2: Passing Raw Pointers from Smart Pointers
```cpp
// Dangerous: Temporary raw pointer
void ProcessRange(nsIDOMRange* range);

// Don't do this:
ProcessRange(mCurrentRange);  // nsCOMPtr -> raw pointer temporarily

// Better: Use get() explicitly
ProcessRange(mCurrentRange.get());

// Best: Update ProcessRange to take smart pointer
void ProcessRange(nsCOMPtr<nsIDOMRange> range);
```

### Pitfall 3: Forgetting to Update All Code Paths
```cpp
// Make sure all constructors, assignment operators, etc. are updated
class nsSelection {
  nsCOMPtr<nsIDOMRange> mRange;
  
public:
  nsSelection() = default;
  nsSelection(const nsSelection& other) : mRange(other.mRange) {}  // Copy constructor
  nsSelection& operator=(const nsSelection& other) {              // Assignment operator
    mRange = other.mRange;
    return *this;
  }
};
```

## References

- [nsCOMPtr Guide](https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide/nsCOMPtr)
- [RefPtr Documentation](https://developer.mozilla.org/en-US/docs/Mozilla/C++_Guide/RefPtr)
- [std::unique_ptr Reference](https://en.cppreference.com/w/cpp/memory/unique_ptr)
- [std::shared_ptr Reference](https://en.cppreference.com/w/cpp/memory/shared_ptr)
- [RAII and Smart Pointers](https://en.cppreference.com/w/cpp/memory)