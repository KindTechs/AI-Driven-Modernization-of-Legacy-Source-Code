/*
 * Modernized nsSelection Implementation
 * 
 * This file demonstrates the modernization of Mozilla 1.0 nsSelection methods
 * using modern C++ patterns and practices.
 */

#include "nsSelection_modernized.h"
#include "mozilla/Result.h"
#include "mozilla/Maybe.h"
#include <memory>
#include <optional>

// Forward declarations for compatibility
class nsIDOMRange;
class nsISupportsArray;

using mozilla::Result;
using mozilla::Ok;
using mozilla::Err;
using mozilla::Maybe;
using mozilla::Some;
using mozilla::Nothing;

namespace mozilla {

// Modern nsSelection class with improved patterns
class nsTypedSelectionModern {
private:
    nsCOMPtr<nsISupportsArray> mRangeArray;
    
public:
    // Modern GetRangeAt implementation
    Result<nsCOMPtr<nsIDOMRange>, nsresult> GetRangeAtModern(int32_t aIndex);
    
    // Modern GetRangeCount implementation
    Result<int32_t, nsresult> GetRangeCountModern();
    
    // Modern AddRange implementation
    Result<void, nsresult> AddRangeModern(nsCOMPtr<nsIDOMRange> aRange);
    
    // Modern RemoveRange implementation
    Result<void, nsresult> RemoveRangeModern(nsCOMPtr<nsIDOMRange> aRange);
    
    // Modern GetIsCollapsed implementation
    Result<bool, nsresult> GetIsCollapsedModern();
    
    // Compatibility wrappers for existing code
    NS_IMETHODIMP GetRangeAt(int32_t aIndex, nsIDOMRange** aReturn);
    NS_IMETHODIMP GetRangeCount(int32_t* aCount);
    NS_IMETHODIMP AddRange(nsIDOMRange* aRange);
    NS_IMETHODIMP RemoveRange(nsIDOMRange* aRange);
    NS_IMETHODIMP GetIsCollapsed(bool* aIsCollapsed);
};

// ==================================================================
// MODERN IMPLEMENTATION
// ==================================================================

Result<nsCOMPtr<nsIDOMRange>, nsresult>
nsTypedSelectionModern::GetRangeAtModern(int32_t aIndex)
{
    // Validate range array exists
    if (!mRangeArray) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }
    
    // Get count using modern implementation
    auto countResult = GetRangeCountModern();
    if (countResult.isErr()) {
        return Err(countResult.unwrapErr());
    }
    
    int32_t count = countResult.unwrap();
    
    // Validate index bounds
    if (aIndex < 0 || aIndex >= count) {
        return Err(NS_ERROR_INVALID_ARG);
    }
    
    // Get element from array
    nsISupports* element = mRangeArray->ElementAt(static_cast<uint32_t>(aIndex));
    if (!element) {
        return Err(NS_ERROR_UNEXPECTED);
    }
    
    // Query interface to get DOM range
    nsCOMPtr<nsIDOMRange> range = do_QueryInterface(element);
    if (!range) {
        return Err(NS_ERROR_NO_INTERFACE);
    }
    
    return Ok(range);
}

Result<int32_t, nsresult>
nsTypedSelectionModern::GetRangeCountModern()
{
    if (!mRangeArray) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }
    
    uint32_t count;
    nsresult rv = mRangeArray->Count(&count);
    if (NS_FAILED(rv)) {
        return Err(rv);
    }
    
    // Safe conversion from uint32_t to int32_t
    if (count > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        return Err(NS_ERROR_INVALID_ARG);
    }
    
    return Ok(static_cast<int32_t>(count));
}

Result<void, nsresult>
nsTypedSelectionModern::AddRangeModern(nsCOMPtr<nsIDOMRange> aRange)
{
    if (!aRange) {
        return Err(NS_ERROR_INVALID_ARG);
    }
    
    if (!mRangeArray) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }
    
    // Check if range already exists
    auto countResult = GetRangeCountModern();
    if (countResult.isErr()) {
        return Err(countResult.unwrapErr());
    }
    
    int32_t count = countResult.unwrap();
    for (int32_t i = 0; i < count; ++i) {
        auto existingRangeResult = GetRangeAtModern(i);
        if (existingRangeResult.isOk()) {
            nsCOMPtr<nsIDOMRange> existingRange = existingRangeResult.unwrap();
            if (existingRange == aRange) {
                return Ok(); // Range already exists, nothing to do
            }
        }
    }
    
    // Add the range
    nsresult rv = mRangeArray->AppendElement(aRange);
    if (NS_FAILED(rv)) {
        return Err(rv);
    }
    
    return Ok();
}

Result<void, nsresult>
nsTypedSelectionModern::RemoveRangeModern(nsCOMPtr<nsIDOMRange> aRange)
{
    if (!aRange) {
        return Err(NS_ERROR_INVALID_ARG);
    }
    
    if (!mRangeArray) {
        return Err(NS_ERROR_NOT_INITIALIZED);
    }
    
    // Find and remove the range
    auto countResult = GetRangeCountModern();
    if (countResult.isErr()) {
        return Err(countResult.unwrapErr());
    }
    
    int32_t count = countResult.unwrap();
    for (int32_t i = 0; i < count; ++i) {
        auto existingRangeResult = GetRangeAtModern(i);
        if (existingRangeResult.isOk()) {
            nsCOMPtr<nsIDOMRange> existingRange = existingRangeResult.unwrap();
            if (existingRange == aRange) {
                nsresult rv = mRangeArray->RemoveElementAt(static_cast<uint32_t>(i));
                if (NS_FAILED(rv)) {
                    return Err(rv);
                }
                return Ok();
            }
        }
    }
    
    return Err(NS_ERROR_INVALID_ARG); // Range not found
}

Result<bool, nsresult>
nsTypedSelectionModern::GetIsCollapsedModern()
{
    auto countResult = GetRangeCountModern();
    if (countResult.isErr()) {
        return Err(countResult.unwrapErr());
    }
    
    int32_t count = countResult.unwrap();
    
    // Selection is collapsed if it has no ranges or only empty ranges
    if (count == 0) {
        return Ok(true);
    }
    
    if (count > 1) {
        return Ok(false); // Multiple ranges = not collapsed
    }
    
    // Check if the single range is collapsed
    auto rangeResult = GetRangeAtModern(0);
    if (rangeResult.isErr()) {
        return Err(rangeResult.unwrapErr());
    }
    
    nsCOMPtr<nsIDOMRange> range = rangeResult.unwrap();
    bool isCollapsed;
    nsresult rv = range->GetCollapsed(&isCollapsed);
    if (NS_FAILED(rv)) {
        return Err(rv);
    }
    
    return Ok(isCollapsed);
}

// ==================================================================
// COMPATIBILITY WRAPPERS
// ==================================================================

NS_IMETHODIMP
nsTypedSelectionModern::GetRangeAt(int32_t aIndex, nsIDOMRange** aReturn)
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

NS_IMETHODIMP
nsTypedSelectionModern::GetRangeCount(int32_t* aCount)
{
    if (!aCount) {
        return NS_ERROR_NULL_POINTER;
    }
    *aCount = 0;
    
    auto result = GetRangeCountModern();
    if (result.isErr()) {
        return result.unwrapErr();
    }
    
    *aCount = result.unwrap();
    return NS_OK;
}

NS_IMETHODIMP
nsTypedSelectionModern::AddRange(nsIDOMRange* aRange)
{
    if (!aRange) {
        return NS_ERROR_NULL_POINTER;
    }
    
    nsCOMPtr<nsIDOMRange> range = aRange;
    auto result = AddRangeModern(range);
    if (result.isErr()) {
        return result.unwrapErr();
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsTypedSelectionModern::RemoveRange(nsIDOMRange* aRange)
{
    if (!aRange) {
        return NS_ERROR_NULL_POINTER;
    }
    
    nsCOMPtr<nsIDOMRange> range = aRange;
    auto result = RemoveRangeModern(range);
    if (result.isErr()) {
        return result.unwrapErr();
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsTypedSelectionModern::GetIsCollapsed(bool* aIsCollapsed)
{
    if (!aIsCollapsed) {
        return NS_ERROR_NULL_POINTER;
    }
    *aIsCollapsed = false;
    
    auto result = GetIsCollapsedModern();
    if (result.isErr()) {
        return result.unwrapErr();
    }
    
    *aIsCollapsed = result.unwrap();
    return NS_OK;
}

// ==================================================================
// USAGE EXAMPLES
// ==================================================================

namespace Examples {

// Example 1: Modern usage with error handling
void ModernUsageExample(nsTypedSelectionModern& selection) {
    // Modern way - explicit error handling
    auto result = selection.GetRangeAtModern(0);
    if (result.isErr()) {
        // Handle specific error
        nsresult error = result.unwrapErr();
        if (error == NS_ERROR_INVALID_ARG) {
            // Handle invalid index
        } else if (error == NS_ERROR_NOT_INITIALIZED) {
            // Handle uninitialized selection
        }
        return;
    }
    
    nsCOMPtr<nsIDOMRange> range = result.unwrap();
    if (range) {
        // Use the range safely
        // Automatic cleanup when range goes out of scope
    }
}

// Example 2: Chaining operations
void ChainedOperationsExample(nsTypedSelectionModern& selection) {
    // Check if selection is collapsed
    auto collapsedResult = selection.GetIsCollapsedModern();
    if (collapsedResult.isErr()) {
        return;
    }
    
    bool isCollapsed = collapsedResult.unwrap();
    if (isCollapsed) {
        return;
    }
    
    // Get the first range
    auto rangeResult = selection.GetRangeAtModern(0);
    if (rangeResult.isErr()) {
        return;
    }
    
    nsCOMPtr<nsIDOMRange> range = rangeResult.unwrap();
    // Use the range...
}

// Example 3: Legacy compatibility
void LegacyCompatibilityExample(nsTypedSelectionModern& selection) {
    // Old style still works
    nsIDOMRange* range = nullptr;
    nsresult rv = selection.GetRangeAt(0, &range);
    if (NS_SUCCEEDED(rv) && range) {
        // Use range...
        range->Release(); // Must remember to release!
    }
}

// Example 4: Range iteration
void IterateRangesExample(nsTypedSelectionModern& selection) {
    auto countResult = selection.GetRangeCountModern();
    if (countResult.isErr()) {
        return;
    }
    
    int32_t count = countResult.unwrap();
    for (int32_t i = 0; i < count; ++i) {
        auto rangeResult = selection.GetRangeAtModern(i);
        if (rangeResult.isOk()) {
            nsCOMPtr<nsIDOMRange> range = rangeResult.unwrap();
            // Process each range...
        }
    }
}

} // namespace Examples

} // namespace mozilla

// ==================================================================
// MODERNIZATION SUMMARY
// ==================================================================

/*
 * MODERNIZATION PATTERNS APPLIED:
 * 
 * 1. ERROR HANDLING:
 *    - Before: nsresult return codes with out parameters
 *    - After: Result<T, nsresult> return types with explicit error handling
 * 
 * 2. MEMORY MANAGEMENT:
 *    - Before: Manual AddRef/Release patterns
 *    - After: nsCOMPtr smart pointers with automatic cleanup
 * 
 * 3. TYPE SAFETY:
 *    - Before: PRInt32/PRUint32 Mozilla types
 *    - After: Standard int32_t/uint32_t with safe conversions
 * 
 * 4. PARAMETER PATTERNS:
 *    - Before: Out parameters with double pointers
 *    - After: Return values with clear ownership semantics
 * 
 * 5. NULL SAFETY:
 *    - Before: Manual null checks throughout
 *    - After: Smart pointers with automatic null handling
 * 
 * 6. BACKWARDS COMPATIBILITY:
 *    - Maintained original APIs through compatibility wrappers
 *    - Allows gradual migration of existing code
 * 
 * BENEFITS ACHIEVED:
 * - Impossible to ignore errors (compile-time enforcement)
 * - Automatic memory management (no leaks)
 * - Clear ownership semantics
 * - Type safety improvements
 * - Reduced boilerplate code
 * - Better composability
 * - Easier testing
 * 
 * COMPATIBILITY:
 * - All existing code continues to work unchanged
 * - New code can use modern patterns
 * - Gradual migration path provided
 */