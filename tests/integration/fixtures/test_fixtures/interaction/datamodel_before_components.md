# datamodel_before_components

## Basic Info

| Property | Value |
|------|------|
| Category | Message Ordering |
| Related Tests | ORDER-01 |
| Surface ID | `test-surf-dm-before-comp` |
| Protocol Version | v0.9 |

## Test Case Description

Verifies the reverse-order scenario where `updateDataModel` arrives before `updateComponents`: the engine should correctly resolve existing DataModel binding values when components are created.

The A2UI v0.9 protocol only requires that `createSurface` must precede other messages, without restricting the order of `updateDataModel` and `updateComponents`. This test case specifically tests the "data before components" edge scenario to ensure the engine handles both message orderings correctly.

Test focus:
- When `updateDataModel` arrives first, data is already written to DataModel
- Subsequent `updateComponents` DataBindings (template string `${/path}`) and FunctionCalls (formatString / formatCurrency / pluralize) can correctly resolve existing data
- Component tree renders completely with correct binding values

## Protocol Messages

Send 3 messages (note the reverse order):
1. `createSurface` - Create Surface
2. `updateDataModel` - Updates data model **first** (user, order data)
3. `updateComponents` - Renders component tree with DataBindings **after**

## Component Tree Structure

```
Column (root)
├── Text (title) - "Order Confirmation" [variant: h2]
├── Text (greeting) - formatString("Hello, ${/user/name}!") → "Hello, Bob!" [variant: body]
├── Text (order-summary) - formatCurrency(/order/total, USD) → "$199.50" [variant: body]
└── Text (item-count) - pluralize(/order/count, "item"/"items") → "items" [variant: body]
```

## DataModel Content

```json
{
  "user": { "name": "Bob", "age": 30 },
  "order": { "total": 199.5, "count": 3 }
}
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 5 |
| All component IDs | `["root", "title", "greeting", "order-summary", "item-count"]` |
| greeting text | Contains `"Bob"` (formatString embeds DataBinding /user/name) |
| order-summary text | Contains `"199.50"` (DataBinding /order/total -> formatCurrency USD) |
| item-count text | Contains `"items"` (DataBinding /order/count=3 -> pluralize other branch) |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 3 messages in reverse order: createSurface -> updateDataModel -> updateComponents
3. Wait for Surface and component rendering to complete
4. Verify total components equals 5
5. Verify each Text component's text content contains correct values resolved from DataBinding

## Notes

- This test case complements other fixtures that use standard order (updateComponents -> updateDataModel)
- If the engine doesn't support reverse-order scenarios (FunctionCall cannot resolve when DataModel is empty), this test case will expose the issue
- For standard-order DataModel scenario, see `stream/cases/03_with_datamodel.json`

## Platform Coverage

- Android: To be verified
- iOS: To be verified (iOS tests do not currently load this fixture independently)
- Harmony: To be verified
