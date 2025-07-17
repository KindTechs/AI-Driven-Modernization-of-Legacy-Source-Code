# Out Parameter to Return Value Modernization Template

## Overview
This template demonstrates how to convert Mozilla 1.0 out parameter patterns to modern return value patterns, improving code clarity and type safety while reducing the potential for parameter-related errors.

## Pattern Description
Mozilla 1.0 frequently uses out parameters (typically `Type**` or `Type*&`) to return values from functions, combined with nsresult return codes for error handling. This pattern is error-prone and makes code harder to read and maintain.

## Before (Legacy Pattern)
```cpp
// Out parameter pattern - error-prone and verbose
NS_IMETHODIMP
nsSelection::GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  *aReturn = nsnull;
  
  if (aIndex < 0 || aIndex >= (PRInt32)mRanges.Length())
    return NS_ERROR_INVALID_ARG;
  
  if (mRanges[aIndex]) {
    nsresult rv = CallQueryInterface(mRanges[aIndex], aReturn);
    return rv;
  }
  
  return NS_OK;
}

// Multiple out parameters - even more complex
NS_IMETHODIMP
nsSelection::GetRangeInfo(PRInt32 aIndex, 
                         nsIDOMRange** aRange,
                         PRInt32* aStartOffset,
                         PRInt32* aEndOffset)
{
  if (!aRange || !aStartOffset || !aEndOffset)
    return NS_ERROR_NULL_POINTER;
    
  *aRange = nsnull;
  *aStartOffset = 0;
  *aEndOffset = 0;
  
  // Complex logic with multiple error paths
  nsresult rv = GetRangeAt(aIndex, aRange);
  if (NS_FAILED(rv)) return rv;
  
  rv = (*aRange)->GetStartOffset(aStartOffset);
  if (NS_FAILED(rv)) {
    (*aRange)->Release();
    *aRange = nsnull;
    return rv;
  }
  
  rv = (*aRange)->GetEndOffset(aEndOffset);
  if (NS_FAILED(rv)) {
    (*aRange)->Release();
    *aRange = nsnull;
    return rv;
  }
  
  return NS_OK;
}

// Usage - verbose and error-prone
nsIDOMRange* range = nullptr;
nsresult rv = selection->GetRangeAt(0, &range);
if (NS_FAILED(rv)) {
  // Handle error
  return rv;
}
// Use range, must remember to Release!
if (range) {
  range->Release();
}
```

## After (Modern Pattern)
```cpp
// Return value pattern - clean and safe
Result<nsCOMPtr<nsIDOMRange>, nsresult>
nsSelection::GetRangeAtModern(int32_t aIndex)
{
  if (aIndex < 0 || aIndex >= static_cast<int32_t>(mRanges.Length())) {
    return Err(NS_ERROR_INVALID_ARG);
  }
  
  if (!mRanges[aIndex]) {
    return Ok(nullptr);
  }
  
  nsCOMPtr<nsIDOMRange> range = do_QueryInterface(mRanges[aIndex]);
  if (!range) {
    return Err(NS_ERROR_NO_INTERFACE);
  }
  
  return Ok(range);
}

// Multiple return values using struct
struct RangeInfo {
  nsCOMPtr<nsIDOMRange> range;
  int32_t startOffset;
  int32_t endOffset;
};

Result<RangeInfo, nsresult>
nsSelection::GetRangeInfoModern(int32_t aIndex)
{
  auto rangeResult = GetRangeAtModern(aIndex);
  if (rangeResult.isErr()) {
    return Err(rangeResult.unwrapErr());
  }
  
  nsCOMPtr<nsIDOMRange> range = rangeResult.unwrap();
  if (!range) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }
  
  int32_t startOffset, endOffset;
  nsresult rv = range->GetStartOffset(&startOffset);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  
  rv = range->GetEndOffset(&endOffset);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  
  return Ok(RangeInfo{range, startOffset, endOffset});
}

// Compatibility wrappers for existing code
NS_IMETHODIMP
nsSelection::GetRangeAt(int32_t aIndex, nsIDOMRange** aReturn)
{
  if (!aReturn) {
    return NS_ERROR_NULL_POINTER;
  }
  *aReturn = nullptr;
  
  auto result = GetRangeAtModern(aIndex);
  if (result.isErr()) {
    return result.unwrapErr();
  }
  
  nsCOMPtr<nsIDOMRange> range = result.unwrap();
  if (range) {
    range.forget(aReturn);
  }
  return NS_OK;
}

// Modern usage - clean and safe
auto result = selection->GetRangeAtModern(0);
if (result.isErr()) {
  // Handle error
  return result.unwrapErr();
}
nsCOMPtr<nsIDOMRange> range = result.unwrap();
// Use range - automatic cleanup
```

## Step-by-Step Implementation

### Step 1: Identify Out Parameter Patterns
```cpp
// Look for these patterns:
nsresult Method(Type** aOut);
nsresult Method(Type*& aOut);
nsresult Method(Type* aOut);  // For primitive types
nsresult Method(Type** aOut1, Type** aOut2);  // Multiple out params
```

### Step 2: Analyze Return Value Requirements
```cpp
// Single return value
nsresult GetSomething(Type** aOut);
// Convert to: Result<Type, nsresult> GetSomething();

// Multiple return values
nsresult GetMultiple(Type1** aOut1, Type2** aOut2);
// Convert to: Result<struct{Type1, Type2}, nsresult> GetMultiple();

// Optional return value
nsresult GetOptional(Type** aOut);  // aOut can be null
// Convert to: Result<Maybe<Type>, nsresult> GetOptional();
```

### Step 3: Create Return Value Structure
```cpp
// For single values - use the type directly
Result<nsCOMPtr<nsIDOMRange>, nsresult> GetRange();

// For multiple values - create a struct
struct SelectionBounds {
  nsCOMPtr<nsIDOMRange> range;
  int32_t startOffset;
  int32_t endOffset;
  
  SelectionBounds(nsCOMPtr<nsIDOMRange> r, int32_t start, int32_t end)
    : range(r), startOffset(start), endOffset(end) {}
};

Result<SelectionBounds, nsresult> GetSelectionBounds();

// For optional values - use Maybe
Result<Maybe<nsCOMPtr<nsIDOMRange>>, nsresult> GetOptionalRange();
```

### Step 4: Implement Modern Method
```cpp
// Before - out parameter version
nsresult GetCurrentRange(nsIDOMRange** aRange, bool* aIsCollapsed) {
  if (!aRange || !aIsCollapsed) return NS_ERROR_NULL_POINTER;
  *aRange = nullptr;
  *aIsCollapsed = false;
  
  // Implementation...
  return NS_OK;
}

// After - return value version
struct RangeResult {
  nsCOMPtr<nsIDOMRange> range;
  bool isCollapsed;
};

Result<RangeResult, nsresult> GetCurrentRangeModern() {
  // Implementation without out parameter handling
  nsCOMPtr<nsIDOMRange> range = GetInternalRange();
  if (!range) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }
  
  bool isCollapsed = CheckIfCollapsed(range);
  return Ok(RangeResult{range, isCollapsed});
}
```

### Step 5: Create Compatibility Wrapper
```cpp
// Maintain old API for backward compatibility
NS_IMETHODIMP
nsSelection::GetCurrentRange(nsIDOMRange** aRange, bool* aIsCollapsed)
{
  if (!aRange || !aIsCollapsed) {
    return NS_ERROR_NULL_POINTER;
  }
  *aRange = nullptr;
  *aIsCollapsed = false;
  
  auto result = GetCurrentRangeModern();
  if (result.isErr()) {
    return result.unwrapErr();
  }
  
  auto rangeResult = result.unwrap();
  if (rangeResult.range) {
    rangeResult.range.forget(aRange);
  }
  *aIsCollapsed = rangeResult.isCollapsed;
  
  return NS_OK;
}
```

### Step 6: Update Callers Gradually
```cpp
// Phase 1: Keep using old API
nsIDOMRange* range = nullptr;
bool isCollapsed = false;
nsresult rv = selection->GetCurrentRange(&range, &isCollapsed);
if (NS_FAILED(rv)) return rv;

// Phase 2: Use modern API
auto result = selection->GetCurrentRangeModern();
if (result.isErr()) return result.unwrapErr();
auto rangeResult = result.unwrap();
// Use rangeResult.range and rangeResult.isCollapsed

// Phase 3: Use with modern error handling
auto result = selection->GetCurrentRangeModern();
MOZ_TRY_VAR(rangeResult, result);
```

## Return Value Patterns

### Pattern 1: Single Value Return
```cpp
// Before
nsresult GetCount(PRInt32* aCount);

// After
Result<int32_t, nsresult> GetCount();
```

### Pattern 2: Multiple Values Return
```cpp
// Before
nsresult GetBounds(PRInt32* aX, PRInt32* aY, PRInt32* aWidth, PRInt32* aHeight);

// After
struct Bounds {
  int32_t x, y, width, height;
};
Result<Bounds, nsresult> GetBounds();
```

### Pattern 3: Optional Value Return
```cpp
// Before
nsresult GetOptionalValue(Type** aValue);  // aValue can be null on success

// After
Result<Maybe<Type>, nsresult> GetOptionalValue();
```

### Pattern 4: Array Return
```cpp
// Before
nsresult GetItems(PRInt32* aCount, Item*** aItems);

// After
Result<nsTArray<RefPtr<Item>>, nsresult> GetItems();
```

### Pattern 5: String Return
```cpp
// Before
nsresult GetString(nsAString& aString);

// After
Result<nsString, nsresult> GetString();
```

## Compatibility Considerations

### Existing Code Compatibility
- **Maintain original APIs**: Keep old methods for backward compatibility
- **Identical behavior**: Ensure same error codes and edge cases
- **Gradual migration**: Allow mixed usage patterns

### Performance Considerations
- **Return value optimization**: Modern compilers optimize return values
- **Move semantics**: Use std::move for expensive objects
- **Stack allocation**: Prefer stack-allocated structures

### Memory Management
- **Smart pointers**: Use nsCOMPtr/RefPtr for automatic cleanup
- **RAII**: Ensure proper resource management
- **Exception safety**: Handle exceptions properly in modern code

## Benefits of Modernization

### Code Clarity
- **Self-documenting**: Return type clearly shows what's returned
- **Reduced boilerplate**: No null checking of out parameters
- **Functional style**: Easier to compose and chain operations

### Type Safety
- **Compile-time checking**: Impossible to ignore return values
- **Clear ownership**: Return values have clear ownership semantics
- **Reduced errors**: Less chance of parameter-related mistakes

### Maintainability
- **Simpler testing**: Easier to test functions with return values
- **Better composition**: Functions can be easily composed
- **Reduced complexity**: Fewer error paths to handle

## Common Patterns and Solutions

### Pattern 1: Conditional Return
```cpp
// Before
nsresult GetItem(PRInt32 aIndex, Item** aItem) {
  if (aIndex >= 0 && aIndex < mItems.Length()) {
    *aItem = mItems[aIndex];
    (*aItem)->AddRef();
  } else {
    *aItem = nullptr;
  }
  return NS_OK;
}

// After
Result<Maybe<RefPtr<Item>>, nsresult> GetItem(int32_t aIndex) {
  if (aIndex >= 0 && aIndex < static_cast<int32_t>(mItems.Length())) {
    return Ok(Some(mItems[aIndex]));
  }
  return Ok(Nothing());
}
```

### Pattern 2: Complex Data Structures
```cpp
// Before
nsresult GetSelectionState(nsIDOMRange** aRange, 
                          PRInt32* aRangeCount,
                          PRBool* aIsCollapsed);

// After
struct SelectionState {
  nsCOMPtr<nsIDOMRange> currentRange;
  int32_t rangeCount;
  bool isCollapsed;
};
Result<SelectionState, nsresult> GetSelectionState();
```

### Pattern 3: Error Differentiation
```cpp
// Before - different error codes for different failure modes
nsresult GetValue(Type** aValue) {
  if (!aValue) return NS_ERROR_NULL_POINTER;
  if (mNotInitialized) return NS_ERROR_NOT_INITIALIZED;
  if (mNoValue) return NS_ERROR_NOT_AVAILABLE;
  // ...
}

// After - preserve error differentiation
Result<Type, nsresult> GetValue() {
  if (mNotInitialized) return Err(NS_ERROR_NOT_INITIALIZED);
  if (mNoValue) return Err(NS_ERROR_NOT_AVAILABLE);
  // ...
}
```

## Testing Strategy

### Unit Tests
```cpp
TEST(OutParameterModernization, BasicReturnValue) {
  nsSelection selection;
  
  // Test modern API
  auto result = selection.GetRangeAtModern(0);
  ASSERT_TRUE(result.isOk());
  
  nsCOMPtr<nsIDOMRange> range = result.unwrap();
  ASSERT_TRUE(range);
}

TEST(OutParameterModernization, ErrorHandling) {
  nsSelection selection;
  
  // Test error cases
  auto result = selection.GetRangeAtModern(-1);
  ASSERT_TRUE(result.isErr());
  ASSERT_EQ(result.unwrapErr(), NS_ERROR_INVALID_ARG);
}

TEST(OutParameterModernization, BackwardCompatibility) {
  nsSelection selection;
  
  // Test that old API still works
  nsIDOMRange* range = nullptr;
  nsresult rv = selection.GetRangeAt(0, &range);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  
  // Test that results are identical
  auto modernResult = selection.GetRangeAtModern(0);
  ASSERT_TRUE(modernResult.isOk());
  ASSERT_EQ(range, modernResult.unwrap().get());
  
  if (range) range->Release();
}
```

### Integration Tests
```cpp
TEST(OutParameterModernization, ComplexStructures) {
  nsSelection selection;
  
  auto result = selection.GetRangeInfoModern(0);
  ASSERT_TRUE(result.isOk());
  
  auto info = result.unwrap();
  ASSERT_TRUE(info.range);
  ASSERT_GE(info.startOffset, 0);
  ASSERT_GE(info.endOffset, info.startOffset);
}
```

## Migration Strategies

### Strategy 1: Method-by-Method
1. Convert one method at a time
2. Create modern version alongside old version
3. Gradually migrate callers
4. Eventually deprecate old version

### Strategy 2: Interface-First
1. Define new interface with return values
2. Implement new interface
3. Create adapter for old interface
4. Migrate callers to new interface

### Strategy 3: Staged Migration
1. Phase 1: Create modern methods
2. Phase 2: Update internal callers
3. Phase 3: Update external callers
4. Phase 4: Remove old methods

## Migration Checklist

- [ ] Identify all out parameter methods
- [ ] Analyze return value requirements
- [ ] Create appropriate return structures
- [ ] Implement modern methods
- [ ] Create compatibility wrappers
- [ ] Update unit tests
- [ ] Test backward compatibility
- [ ] Document new APIs
- [ ] Plan caller migration
- [ ] Create deprecation timeline

## Common Pitfalls

### Pitfall 1: Changing Error Semantics
```cpp
// Bad: Changing what constitutes an error
nsresult GetValue(Type** aValue) {
  if (!found) {
    *aValue = nullptr;
    return NS_OK;  // Not found is not an error
  }
}

// Don't change this to:
Result<Type, nsresult> GetValue() {
  if (!found) {
    return Err(NS_ERROR_NOT_FOUND);  // Changes semantics!
  }
}
```

### Pitfall 2: Performance Regression
```cpp
// Bad: Copying large objects
Result<LargeObject, nsresult> GetLargeObject();

// Better: Use move semantics or references
Result<std::unique_ptr<LargeObject>, nsresult> GetLargeObject();
```

### Pitfall 3: Over-Complex Return Types
```cpp
// Bad: Too many fields in return struct
struct ComplexReturn {
  Type1 field1;
  Type2 field2;
  Type3 field3;
  // ... 20 more fields
};

// Better: Break into smaller methods
Result<Type1, nsresult> GetField1();
Result<Type2, nsresult> GetField2();
```

## References

- [Mozilla Result Type Documentation](https://developer.mozilla.org/en-US/docs/Mozilla/C++_Guide/Result)
- [C++ Core Guidelines - Return Values](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-return-value)
- [Modern C++ Return Value Optimization](https://en.cppreference.com/w/cpp/language/return_value_optimization)
- [std::optional Reference](https://en.cppreference.com/w/cpp/utility/optional)