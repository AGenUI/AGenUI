# A2UI Go Agent SDK

[中文文档](README.zh-CN.md)

The `agent_sdks/go` directory contains the Go implementation of the A2UI agent SDK.

## What is the A2UI Go Agent SDK?

The A2UI (Agent-to-UI) protocol defines a standard way for AI agents to generate rich, interactive user interfaces. This Go SDK enables server-side agents to:

- **Generate structured UI responses** — produce A2UI protocol messages (createSurface, updateComponents, updateDataModel, deleteSurface) that any compliant renderer (Android, iOS, HarmonyOS, Web) can display.
- **Stream UI incrementally** — parse and yield partial UI components in real time as the LLM generates tokens, enabling low-latency progressive rendering.
- **Validate output** — check A2UI messages against JSON schemas and protocol rules (topology, reachability, circular references, recursion depth) to catch errors before they reach the client.
- **Manage component catalogs** — load built-in or custom catalogs and generate LLM system prompts that teach the model which UI components are available and how to use them.

In short, it is the bridge between an LLM backend and any A2UI-compatible front-end renderer.

## Installation

```bash
go get github.com/AGenUI/AGenUI/agent_sdks/go
```

## Usage

For a complete working example, see the [Rizzcharts sample](../../samples/go/rizzcharts/).

## Core Components

### Package `a2ui` (root)

* **`a2ui.go`**: Package entry point. Defines `Version`, `SystemPromptOptions`,
  and the `InferenceStrategy` interface for generating LLM system prompts.

### Schema Management (`schema/`)

* **`manager.go`**: The `A2uiSchemaManager` handles loading specification
  schemas, managing catalogs, and generating system prompts for LLMs.
* **`validator.go`**: Implements `A2uiValidator` for validating A2UI messages
  against JSON schemas and protocol rules (topology, reachability, circular
  references, recursion depth).
* **`catalog.go`**: Defines `A2uiCatalog` and `CatalogConfig` for handling
  component libraries.
* **`catalog_provider.go`**: Provides `A2uiCatalogProvider` interface with
  `FileSystemCatalogProvider` and `RawCatalogProvider` implementations.
* **`common_modifiers.go`**: Schema modifier utilities for pruning and
  customizing component schemas.
* **`assets.go`**: Bundled JSON schema resources (embedded via `go:embed`).
* **`utils.go`**: Shared utility functions for topology analysis and ref field
  extraction.

### Parser (`parser/`)

* **`parser.go`**: Implementation of `ParseResponse` for synchronous parsing of
  complete LLM responses.
* **`streaming.go`**: `A2uiStreamParser` for incremental streaming parsing with
  automatic JSON healing, component-level yielding, and validation.
* **`streaming_v09.go`**: v0.9-specific streaming parser logic
  (`createSurface`, `updateComponents`, `updateDataModel`, `deleteSurface`).
* **`payload_fixer.go`**: Utilities to automatically correct common LLM output
  issues in A2UI payloads (trailing commas, unquoted keys, etc.).
* **`response_part.go`**: Defines the `ResponsePart` struct representing parsed
  output (text + A2UI JSON).

### Basic Catalog (`basiccatalog/`)

* **`basiccatalog.go`**: Implementation of `BundledCatalogProvider` for the
  standard A2UI basic catalog (Button, Text, Row, Column, etc.).

## Running Tests

1. Navigate to the directory:

   ```bash
   cd agent_sdks/go
   ```

2. Run all tests:

   ```bash
   go test ./...
   ```
