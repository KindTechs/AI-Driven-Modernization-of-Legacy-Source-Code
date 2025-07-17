/*
 * Modernized nsSelection Header
 * 
 * This header demonstrates modern C++ patterns for Mozilla selection implementation
 */

#ifndef nsSelection_modernized_h
#define nsSelection_modernized_h

#include "nsCOMPtr.h"
#include "nsIDOMRange.h"
#include "nsISupportsArray.h"
#include "mozilla/Result.h"
#include "mozilla/Maybe.h"
#include <cstdint>
#include <memory>
#include <limits>

// Mozilla standard macros
#define NS_IMETHODIMP nsresult
#define NS_OK 0
#define NS_ERROR_NULL_POINTER 0x80004003
#define NS_ERROR_INVALID_ARG 0x80070057
#define NS_ERROR_NOT_INITIALIZED 0x80004001
#define NS_ERROR_UNEXPECTED 0x8000ffff
#define NS_ERROR_NO_INTERFACE 0x80004002
#define NS_SUCCEEDED(x) ((x) == NS_OK)
#define NS_FAILED(x) ((x) != NS_OK)

using mozilla::Result;
using mozilla::Ok;
using mozilla::Err;
using mozilla::Maybe;
using mozilla::Some;
using mozilla::Nothing;

namespace mozilla {

/**
 * Modern nsSelection class demonstrating contemporary C++ patterns
 * for Mozilla 1.0 selection modernization.
 */
class nsTypedSelectionModern {
private:
    nsCOMPtr<nsISupportsArray> mRangeArray;
    
public:
    // Constructor
    nsTypedSelectionModern();
    
    // Destructor (using default - smart pointers clean up automatically)
    ~nsTypedSelectionModern() = default;
    
    // Copy constructor (modern semantics)
    nsTypedSelectionModern(const nsTypedSelectionModern& other);
    
    // Move constructor (modern C++)
    nsTypedSelectionModern(nsTypedSelectionModern&& other) noexcept;
    
    // Copy assignment operator
    nsTypedSelectionModern& operator=(const nsTypedSelectionModern& other);
    
    // Move assignment operator
    nsTypedSelectionModern& operator=(nsTypedSelectionModern&& other) noexcept;
    
    // ==================================================================
    // MODERN API (using Result<T> and smart pointers)
    // ==================================================================
    
    /**
     * Get a range at the specified index.
     * 
     * @param aIndex The index of the range to retrieve (0-based)
     * @return Result containing the range on success, or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     * - NS_ERROR_INVALID_ARG: Index out of bounds
     * - NS_ERROR_UNEXPECTED: Internal error retrieving element
     * - NS_ERROR_NO_INTERFACE: Element is not a DOM range
     */
    Result<nsCOMPtr<nsIDOMRange>, nsresult> GetRangeAtModern(int32_t aIndex);
    
    /**
     * Get the number of ranges in the selection.
     * 
     * @return Result containing the count on success, or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     * - NS_ERROR_INVALID_ARG: Count exceeds int32_t range
     */
    Result<int32_t, nsresult> GetRangeCountModern();
    
    /**
     * Add a range to the selection.
     * 
     * @param aRange The range to add (must not be null)
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_INVALID_ARG: Range is null
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<void, nsresult> AddRangeModern(nsCOMPtr<nsIDOMRange> aRange);
    
    /**
     * Remove a range from the selection.
     * 
     * @param aRange The range to remove (must not be null)
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_INVALID_ARG: Range is null or not found in selection
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<void, nsresult> RemoveRangeModern(nsCOMPtr<nsIDOMRange> aRange);
    
    /**
     * Remove all ranges from the selection.
     * 
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<void, nsresult> RemoveAllRangesModern();
    
    /**
     * Check if the selection is collapsed (empty or single point).
     * 
     * @return Result containing true if collapsed, false otherwise
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<bool, nsresult> GetIsCollapsedModern();
    
    /**
     * Get the anchor node of the selection.
     * 
     * @return Result containing the anchor node, or Nothing if no selection
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<Maybe<nsCOMPtr<nsIDOMNode>>, nsresult> GetAnchorNodeModern();
    
    /**
     * Get the focus node of the selection.
     * 
     * @return Result containing the focus node, or Nothing if no selection
     * 
     * Error codes:
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<Maybe<nsCOMPtr<nsIDOMNode>>, nsresult> GetFocusNodeModern();
    
    /**
     * Collapse the selection to a single point.
     * 
     * @param aParentNode The parent node to collapse to
     * @param aOffset The offset within the parent node
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_INVALID_ARG: Invalid node or offset
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<void, nsresult> CollapseModern(nsCOMPtr<nsIDOMNode> aParentNode, int32_t aOffset);
    
    /**
     * Select all children of the given node.
     * 
     * @param aParentNode The parent node whose children to select
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_INVALID_ARG: Invalid parent node
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     */
    Result<void, nsresult> SelectAllChildrenModern(nsCOMPtr<nsIDOMNode> aParentNode);
    
    /**
     * Extend the selection to a specific point.
     * 
     * @param aParentNode The parent node to extend to
     * @param aOffset The offset within the parent node
     * @return Result indicating success or error code on failure
     * 
     * Error codes:
     * - NS_ERROR_INVALID_ARG: Invalid parent node or offset
     * - NS_ERROR_NOT_INITIALIZED: Selection not properly initialized
     * - NS_ERROR_UNEXPECTED: Internal error during selection extension
     */
    Result<void, nsresult> ExtendModern(nsCOMPtr<nsIDOMNode> aParentNode, int32_t aOffset);
    
    // ==================================================================
    // COMPATIBILITY API (maintains original nsISelection interface)
    // ==================================================================
    
    /**
     * Legacy GetRangeAt method for backward compatibility.
     * 
     * @param aIndex The index of the range to retrieve
     * @param aReturn [out] The retrieved range (caller must Release)
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP GetRangeAt(int32_t aIndex, nsIDOMRange** aReturn);
    
    /**
     * Legacy GetRangeCount method for backward compatibility.
     * 
     * @param aCount [out] The number of ranges
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP GetRangeCount(int32_t* aCount);
    
    /**
     * Legacy AddRange method for backward compatibility.
     * 
     * @param aRange The range to add
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP AddRange(nsIDOMRange* aRange);
    
    /**
     * Legacy RemoveRange method for backward compatibility.
     * 
     * @param aRange The range to remove
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP RemoveRange(nsIDOMRange* aRange);
    
    /**
     * Legacy RemoveAllRanges method for backward compatibility.
     * 
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP RemoveAllRanges();
    
    /**
     * Legacy GetIsCollapsed method for backward compatibility.
     * 
     * @param aIsCollapsed [out] True if selection is collapsed
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP GetIsCollapsed(bool* aIsCollapsed);
    
    /**
     * Legacy GetAnchorNode method for backward compatibility.
     * 
     * @param aAnchorNode [out] The anchor node (caller must Release)
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP GetAnchorNode(nsIDOMNode** aAnchorNode);
    
    /**
     * Legacy GetFocusNode method for backward compatibility.
     * 
     * @param aFocusNode [out] The focus node (caller must Release)
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP GetFocusNode(nsIDOMNode** aFocusNode);
    
    /**
     * Legacy Collapse method for backward compatibility.
     * 
     * @param aParentNode The parent node to collapse to
     * @param aOffset The offset within the parent node
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP Collapse(nsIDOMNode* aParentNode, int32_t aOffset);
    
    /**
     * Legacy SelectAllChildren method for backward compatibility.
     * 
     * @param aParentNode The parent node whose children to select
     * @return NS_OK on success, error code on failure
     */
    NS_IMETHODIMP SelectAllChildren(nsIDOMNode* aParentNode);
    
    // ==================================================================
    // UTILITY METHODS
    // ==================================================================
    
    /**
     * Initialize the selection with a range array.
     * 
     * @param aRangeArray The range array to use
     * @return Result indicating success or error code on failure
     */
    Result<void, nsresult> Initialize(nsCOMPtr<nsISupportsArray> aRangeArray);
    
    /**
     * Check if the selection is properly initialized.
     * 
     * @return True if initialized, false otherwise
     */
    bool IsInitialized() const;
    
    /**
     * Get a string representation of the selection for debugging.
     * 
     * @return String representation
     */
    std::string ToString() const;
};

// ==================================================================
// HELPER CLASSES AND UTILITIES
// ==================================================================

/**
 * RAII helper for selection operations that need to be batched.
 */
class SelectionBatcher {
private:
    nsTypedSelectionModern& mSelection;
    bool mWasBatching;
    
public:
    explicit SelectionBatcher(nsTypedSelectionModern& aSelection);
    ~SelectionBatcher();
    
    // Non-copyable
    SelectionBatcher(const SelectionBatcher&) = delete;
    SelectionBatcher& operator=(const SelectionBatcher&) = delete;
};

/**
 * Iterator for ranges in a selection.
 */
class SelectionRangeIterator {
private:
    nsTypedSelectionModern& mSelection;
    int32_t mCurrentIndex;
    int32_t mCount;
    
public:
    explicit SelectionRangeIterator(nsTypedSelectionModern& aSelection);
    
    bool HasNext() const;
    Result<nsCOMPtr<nsIDOMRange>, nsresult> Next();
    void Reset();
};

/**
 * Modern selection builder using fluent interface.
 */
class SelectionBuilder {
private:
    nsTypedSelectionModern& mSelection;
    
public:
    explicit SelectionBuilder(nsTypedSelectionModern& aSelection);
    
    SelectionBuilder& AddRange(nsCOMPtr<nsIDOMRange> aRange);
    SelectionBuilder& RemoveRange(nsCOMPtr<nsIDOMRange> aRange);
    SelectionBuilder& Collapse(nsCOMPtr<nsIDOMNode> aNode, int32_t aOffset);
    SelectionBuilder& SelectAllChildren(nsCOMPtr<nsIDOMNode> aParentNode);
    
    Result<void, nsresult> Build();
};

// ==================================================================
// MODERNIZATION METRICS
// ==================================================================

/**
 * Metrics for tracking modernization progress.
 */
struct ModernizationMetrics {
    uint32_t totalMethods;
    uint32_t modernizedMethods;
    uint32_t legacyPatterns;
    uint32_t modernPatterns;
    
    double GetModernizationPercentage() const {
        return totalMethods > 0 ? (static_cast<double>(modernizedMethods) / totalMethods) * 100.0 : 0.0;
    }
    
    double GetPatternModernizationPercentage() const {
        uint32_t totalPatterns = legacyPatterns + modernPatterns;
        return totalPatterns > 0 ? (static_cast<double>(modernPatterns) / totalPatterns) * 100.0 : 0.0;
    }
};

/**
 * Get modernization metrics for this class.
 */
ModernizationMetrics GetModernizationMetrics();

} // namespace mozilla

#endif // nsSelection_modernized_h

/*
 * MODERNIZATION DESIGN PRINCIPLES:
 * 
 * 1. SAFETY FIRST:
 *    - Use Result<T> for error handling
 *    - Use smart pointers for memory management
 *    - Use standard types for better safety
 * 
 * 2. BACKWARDS COMPATIBILITY:
 *    - Maintain all original APIs
 *    - Provide compatibility wrappers
 *    - Allow gradual migration
 * 
 * 3. PERFORMANCE:
 *    - Zero-cost abstractions where possible
 *    - Move semantics for expensive operations
 *    - Efficient error handling
 * 
 * 4. MAINTAINABILITY:
 *    - Clear documentation
 *    - Consistent error handling
 *    - Modular design
 * 
 * 5. TESTABILITY:
 *    - Clear interfaces
 *    - Dependency injection
 *    - Mockable components
 */