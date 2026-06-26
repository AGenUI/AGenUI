import Testing
import Foundation
@testable import AGenUI

// MARK: - ImageLoaderError Unit Tests

// ============================================================================
// errorDescription — All Cases
// ============================================================================

@Test func imageLoaderError_invalidURL_returnsCorrectDescription() {
    let error = ImageLoaderError.invalidURL("http://bad url")
    #expect(error.errorDescription == "Invalid URL: http://bad url")
}

@Test func imageLoaderError_invalidURL_emptyString_returnsCorrectDescription() {
    let error = ImageLoaderError.invalidURL("")
    #expect(error.errorDescription == "Invalid URL: ")
}

@Test func imageLoaderError_networkError_returnsCorrectDescription() {
    let underlying = NSError(domain: "NSURLErrorDomain", code: -1009, userInfo: [NSLocalizedDescriptionKey: "The Internet connection appears to be offline."])
    let error = ImageLoaderError.networkError(underlying)
    #expect(error.errorDescription?.contains("Network error") == true)
    #expect(error.errorDescription?.contains("offline") == true)
}

@Test func imageLoaderError_invalidData_returnsCorrectDescription() {
    let error = ImageLoaderError.invalidData
    #expect(error.errorDescription == "Invalid image data")
}

@Test func imageLoaderError_decompressionFailed_returnsCorrectDescription() {
    let error = ImageLoaderError.decompressionFailed
    #expect(error.errorDescription == "Image decompression failed")
}

@Test func imageLoaderError_cancelled_returnsCorrectDescription() {
    let error = ImageLoaderError.cancelled
    #expect(error.errorDescription == "Image loading was cancelled")
}

// ============================================================================
// isCancelled — Convenience Property
// ============================================================================

@Test func imageLoaderError_cancelled_isCancelledReturnsTrue() {
    let error = ImageLoaderError.cancelled
    #expect(error.isCancelled == true)
}

@Test func imageLoaderError_invalidURL_isCancelledReturnsFalse() {
    let error = ImageLoaderError.invalidURL("http://example.com")
    #expect(error.isCancelled == false)
}

@Test func imageLoaderError_networkError_isCancelledReturnsFalse() {
    let underlying = NSError(domain: "Test", code: 1)
    let error = ImageLoaderError.networkError(underlying)
    #expect(error.isCancelled == false)
}

@Test func imageLoaderError_invalidData_isCancelledReturnsFalse() {
    let error = ImageLoaderError.invalidData
    #expect(error.isCancelled == false)
}

@Test func imageLoaderError_decompressionFailed_isCancelledReturnsFalse() {
    let error = ImageLoaderError.decompressionFailed
    #expect(error.isCancelled == false)
}

// ============================================================================
// Equatable — Same Cases
// ============================================================================

@Test func imageLoaderError_sameInvalidURL_areEqual() {
    let a = ImageLoaderError.invalidURL("http://test.com")
    let b = ImageLoaderError.invalidURL("http://test.com")
    #expect(a == b)
}

@Test func imageLoaderError_differentInvalidURL_areNotEqual() {
    let a = ImageLoaderError.invalidURL("http://a.com")
    let b = ImageLoaderError.invalidURL("http://b.com")
    #expect(a != b)
}

@Test func imageLoaderError_networkErrors_areEqual() {
    let e1 = NSError(domain: "A", code: 1)
    let e2 = NSError(domain: "B", code: 2)
    // networkError equality ignores underlying error details
    #expect(ImageLoaderError.networkError(e1) == ImageLoaderError.networkError(e2))
}

@Test func imageLoaderError_invalidData_areEqual() {
    #expect(ImageLoaderError.invalidData == ImageLoaderError.invalidData)
}

@Test func imageLoaderError_decompressionFailed_areEqual() {
    #expect(ImageLoaderError.decompressionFailed == ImageLoaderError.decompressionFailed)
}

@Test func imageLoaderError_cancelled_areEqual() {
    #expect(ImageLoaderError.cancelled == ImageLoaderError.cancelled)
}

// ============================================================================
// Equatable — Different Cases
// ============================================================================

@Test func imageLoaderError_invalidURLVsCancelled_areNotEqual() {
    #expect(ImageLoaderError.invalidURL("x") != ImageLoaderError.cancelled)
}

@Test func imageLoaderError_invalidDataVsDecompression_areNotEqual() {
    #expect(ImageLoaderError.invalidData != ImageLoaderError.decompressionFailed)
}

@Test func imageLoaderError_networkVsCancelled_areNotEqual() {
    let e = NSError(domain: "T", code: 0)
    #expect(ImageLoaderError.networkError(e) != ImageLoaderError.cancelled)
}

// ============================================================================
// Error Protocol Conformance
// ============================================================================

@Test func imageLoaderError_conformsToErrorProtocol() {
    let error: Error = ImageLoaderError.invalidData
    #expect(error.localizedDescription.contains("Invalid image data"))
}
