# Error Code to Result Type Modernization Template

## Overview
This template demonstrates how to convert Mozilla 1.0 error code-based return patterns to modern Result<T> types, improving error handling safety and readability.

## Pattern Description
Mozilla 1.0 frequently uses `nsresult` return codes with out-parameters to return both success/failure information and actual values. This pattern is error-prone and requires manual checking of return values.

## Before (Legacy Pattern)
```cpp
NS_IMETHODIMP
nsSelection::GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  *aReturn = nsnull;
  
  nsresult res = NS_OK;
  if (aIndex < 0 || aIndex >= (PRInt32)mRanges.Length())
    return NS_ERROR_INVALID_ARG;
  
  if (mRanges[aIndex]) {
    res = CallQueryInterface(mRanges[aIndex], aReturn);
  }
  
  return res;
}

// Usage
nsIDOMRange* range = nullptr;
nsresult rv = selection->GetRangeAt(0, &range);
if (NS_FAILED(rv)) {
  // Handle error
  return rv;
}
// Use range...
```

## After (Modern Pattern)
```cpp
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

// Compatibility wrapper for existing code
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

// Modern usage
auto result = selection->GetRangeAtModern(0);
if (result.isErr()) {
  // Handle error
  return result.unwrapErr();
}
nsCOMPtr<nsIDOMRange> range = result.unwrap();
// Use range...
```

## Step-by-Step Implementation

### Step 1: Define the Result Type
```cpp
// Include the Result type header
#include "mozilla/Result.h"

// Define convenience alias if needed
template<typename T>
using SelectionResult = Result<T, nsresult>;
```

### Step 2: Create the Modern Method
1. **Change return type**: Replace `nsresult` with `Result<T, nsresult>`
2. **Remove out-parameters**: Move the out-parameter to the template parameter
3. **Replace error returns**: Use `Err(error_code)` instead of `return error_code`
4. **Replace success returns**: Use `Ok(value)` instead of setting out-parameter and returning `NS_OK`

### Step 3: Handle Edge Cases
```cpp
// Null/empty cases
if (!value) {
  return Ok(nullptr);  // or Err(NS_ERROR_NOT_AVAILABLE) based on semantics
}

// Interface querying
nsCOMPtr<nsIDOMRange> range = do_QueryInterface(rawPtr);
if (!range) {
  return Err(NS_ERROR_NO_INTERFACE);
}
return Ok(range);
```

### Step 4: Create Compatibility Wrapper
Keep the original method signature for backward compatibility:
```cpp
NS_IMETHODIMP
OriginalMethod(params..., OutType** aReturn)
{
  if (!aReturn) {
    return NS_ERROR_NULL_POINTER;
  }
  *aReturn = nullptr;
  
  auto result = ModernMethod(params...);
  if (result.isErr()) {
    return result.unwrapErr();
  }
  
  auto value = result.unwrap();
  if (value) {
    value.forget(aReturn);  // For nsCOMPtr
    // or *aReturn = value.release(); for other smart pointers
  }
  return NS_OK;
}
```

### Step 5: Update Callers (Gradual Migration)
```cpp
// Phase 1: Keep existing callers working
nsIDOMRange* range = nullptr;
nsresult rv = selection->GetRangeAt(0, &range);
if (NS_FAILED(rv)) {
  return rv;
}

// Phase 2: Update to modern pattern
auto result = selection->GetRangeAtModern(0);
if (result.isErr()) {
  return result.unwrapErr();
}
nsCOMPtr<nsIDOMRange> range = result.unwrap();

// Phase 3: Use modern error handling throughout
auto result = selection->GetRangeAtModern(0);
MOZ_TRY_VAR(range, result);  // Propagate errors automatically
```

## Compatibility Considerations

### Existing Code Compatibility
- **Maintain original API**: Keep the original method signature
- **Preserve behavior**: Ensure identical error codes and return values
- **Gradual migration**: Allow mixed usage during transition period

### Performance Considerations
- **Zero-cost abstraction**: Result<T> should have no runtime overhead
- **Move semantics**: Use move constructors for heavy objects
- **Inline wrapper**: Mark compatibility wrapper as inline

### Build System Integration
- **Conditional compilation**: Use `#ifdef MOZ_RESULT_EXTENSIONS` for gradual rollout
- **Header dependencies**: Ensure Result.h is available in build
- **Template instantiation**: Watch for compile-time overhead

## Benefits of Modernization

### Type Safety
- **Compile-time checking**: Impossible to ignore errors
- **Clear ownership**: No ambiguity about who owns returned pointers
- **Explicit null handling**: Clear distinction between errors and null values

### Code Clarity
- **Self-documenting**: Return type clearly indicates success/failure
- **Reduced boilerplate**: No need for manual error checking
- **Functional style**: Enables chaining and composition

### Error Handling
- **Forced error handling**: Compiler enforces error checking
- **Error propagation**: Easy to bubble up errors through call stack
- **Pattern matching**: Can use match-like constructs for handling

## Common Patterns and Solutions

### Pattern 1: Optional Values
```cpp
// Old: Return null on not found
if (!found) {
  *aReturn = nullptr;
  return NS_OK;
}

// New: Use Optional
Result<Maybe<nsCOMPtr<nsIDOMRange>>, nsresult>
GetOptionalRange(int32_t aIndex) {
  if (!found) {
    return Ok(Nothing());
  }
  return Ok(Some(range));
}
```

### Pattern 2: Multiple Error Types
```cpp
// Old: Different error codes
if (invalidIndex) return NS_ERROR_INVALID_ARG;
if (outOfMemory) return NS_ERROR_OUT_OF_MEMORY;

// New: Enum class for error types
enum class SelectionError {
  InvalidIndex,
  OutOfMemory,
  NotFound
};

Result<nsCOMPtr<nsIDOMRange>, SelectionError>
GetRangeAtModern(int32_t aIndex);
```

### Pattern 3: Async Operations
```cpp
// Old: Callback-based
nsresult StartAsyncOperation(nsICallback* aCallback);

// New: Promise-based or Result with continuation
Result<RefPtr<Promise>, nsresult>
StartAsyncOperationModern();
```

## Testing Strategy

### Unit Tests
```cpp
TEST(nsSelection, GetRangeAtModern_ValidIndex) {
  auto result = selection->GetRangeAtModern(0);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(result.unwrap());
}

TEST(nsSelection, GetRangeAtModern_InvalidIndex) {
  auto result = selection->GetRangeAtModern(-1);
  ASSERT_TRUE(result.isErr());
  ASSERT_EQ(result.unwrapErr(), NS_ERROR_INVALID_ARG);
}
```

### Integration Tests
```cpp
TEST(nsSelection, BackwardCompatibility) {
  // Test that old API still works
  nsIDOMRange* range = nullptr;
  nsresult rv = selection->GetRangeAt(0, &range);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  
  // Test that new API produces same results
  auto result = selection->GetRangeAtModern(0);
  ASSERT_TRUE(result.isOk());
  ASSERT_EQ(range, result.unwrap().get());
}
```

## Migration Checklist

- [ ] Identify all methods using error code pattern
- [ ] Create modern versions with Result<T> return types
- [ ] Implement compatibility wrappers
- [ ] Update unit tests to cover both APIs
- [ ] Add integration tests for backward compatibility
- [ ] Update documentation
- [ ] Plan gradual migration of callers
- [ ] Consider deprecation timeline for old API

## References

- [Mozilla Result Type Documentation](https://developer.mozilla.org/en-US/docs/Mozilla/C++_Guide/Result)
- [C++ Expected Proposal](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0323r3.html)
- [Rust Result Type](https://doc.rust-lang.org/std/result/enum.Result.html)
- [nsresult Documentation](https://developer.mozilla.org/en-US/docs/Mozilla/XPCOM/Reference/Core_functions/nsresult)