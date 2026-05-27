# Component Catalog

## Protocol Shape

A2UI separates structure from data:

- `updateComponents`: component tree, layout, styles, binding paths
- `updateDataModel`: data content corresponding to binding paths
- Dynamic content is bound via `{"path": "..."}`

Minimal structure:

```json
{
  "version": "v0.9",
  "updateComponents": {
    "surfaceId": "sample_surface",
    "components": [
      { "id": "root", "component": "Column", "children": ["title"] },
      { "id": "title", "component": "Text", "text": { "path": "/page/title" }, "variant": "h2" }
    ]
  }
}
```

```json
{
  "version": "v0.9",
  "updateDataModel": {
    "surfaceId": "sample_surface",
    "path": "/page",
    "value": {
      "title": "Hello A2UI"
    }
  }
}
```

## Atomic Components

### Layout Components

#### `Column`

Stacks child components vertically.

```json
{"id": "col1", "component": "Column", "children": ["child1", "child2"], "justify": "start|center|end|spaceBetween|spaceAround|spaceEvenly|stretch", "align": "start|center|end|stretch"}
```

#### `Row`

Arranges child components horizontally.

```json
{"id": "row1", "component": "Row", "children": ["child1", "child2"], "justify": "start|center|end|spaceBetween|spaceAround|spaceEvenly|stretch", "align": "start|center|end|stretch"}
```

#### `List`

List component; supports static children or template rendering driven by dynamic data.

```json
{"id": "list1", "component": "List", "children": ["item1", "item2"], "direction": "vertical|horizontal", "align": "start|center|end|stretch"}
```

```json
{"id": "list1", "component": "List", "children": {"path": "/data/items", "componentId": "item_template"}, "direction": "vertical|horizontal", "align": "start|center|end|stretch"}
```

#### `Card`

Card container. `child` accepts only a single component id.

```json
{"id": "card1", "component": "Card", "child": "inner_layout"}
```

#### `Tabs`

Tab panel. `tabs[].child` is a component id.

```json
{"id": "tabs1", "component": "Tabs", "tabs": [{"title": {"path": "/tabs/t1"}, "child": "tab_content_1"}, {"title": {"path": "/tabs/t2"}, "child": "tab_content_2"}]}
```

#### `Divider`

Separator line.

```json
{"id": "div1", "component": "Divider", "axis": "horizontal|vertical"}
```

#### `Modal`

Dialog. `trigger` is the triggering component id; `content` is the dialog body component id.

```json
{"id": "modal1", "component": "Modal", "trigger": "btn1", "content": "modal_body"}
```

#### `Carousel`

Image carousel. `content` is an array of image URLs. `autoplay` defaults to `false`; `draggable` defaults to `true`.

```json
{"id": "carousel1", "component": "Carousel", "content": ["/images/slide1.jpg", "/images/slide2.jpg"], "autoplay": false, "draggable": true}
```

### Content Components

#### `Text`

```json
{"id": "t1", "component": "Text", "text": {"path": "/data/title"}, "variant": "h1|h2|h3|h4|h5|body|caption"}
```

#### `RichText`

Supported HTML tags: `<font>`, `<color>`, `<a>`, `<br>`, `<blockquote>`, `<i>`, `<u>`, `<strike>`, `<sub>`, `<sup>`, `<strong>`, `<b>`, `<small>`, `<img>`. `linksEnable` defaults to `true`.

```json
{"id": "rt1", "component": "RichText", "text": {"path": "/data/richContent"}, "variant": "h1|h2|h3|h4|h5|body|caption", "linksEnable": true}
```

#### `Markdown`

Supports common Markdown syntax and streaming incremental append.

```json
{"id": "md1", "component": "Markdown", "content": {"path": "/data/mdContent"}}
```

#### `Image`

```json
{"id": "img1", "component": "Image", "url": {"path": "/data/imageUrl"}, "variant": "icon|avatar|smallFeature|mediumFeature|largeFeature|header", "fit": "contain|cover|fill|none|scale-down"}
```

#### `Icon`

Built-in icon. `name` supports a literal value or `{"path": "..."}` dynamic binding.

```json
{"id": "icon1", "component": "Icon", "name": "accountCircle|add|arrowBack|arrowForward|attachFile|calendarToday|call|camera|check|close|delete|download|edit|error|event|fastForward|favorite|favoriteOff|folder|help|home|info|locationOn|lock|lockOpen|mail|menu|moreHoriz|moreVert|notifications|notificationsOff|pause|payment|person|phone|photo|play|print|refresh|rewind|search|send|settings|share|shoppingCart|skipNext|skipPrevious|star|starHalf|starOff|stop|upload|visibility|visibilityOff|volumeDown|volumeMute|volumeOff|volumeUp|warning"}
```

#### `Video`

```json
{"id": "v1", "component": "Video", "url": {"path": "/data/videoUrl"}}
```

#### `AudioPlayer`

```json
{"id": "a1", "component": "AudioPlayer", "url": {"path": "/data/audioUrl"}, "description": {"path": "/data/audioDesc"}}
```

#### `Lottie`

`url` supports `http/https`, `res://...`, `file:///...`. `autoPlay` defaults to `true`; `loop` defaults to `true`.

```json
{"id": "lottie1", "component": "Lottie", "url": {"path": "/data/lottieUrl"}, "autoPlay": true, "loop": true, "variant": "icon|small|medium|large"}
```

#### `Web`

`source` supports a URL or an HTML string.

```json
{"id": "web1", "component": "Web", "source": {"path": "/data/webSource"}}
```

### Interaction Components

#### `Button`

`child` accepts only a single component id. `action` supports `functionCall` or `event`.

```json
{"id": "btn1", "component": "Button", "child": "btn_text1", "variant": "primary|borderless", "action": {"functionCall": {"call": "openUrl", "args": {"url": "https://..."}}}, "style": {"background-color": "...", "border-radius": "...", "padding": "..."}}
```

```json
{"id": "btn2", "component": "Button", "child": "btn_text2", "action": {"event": {"name": "submit", "context": {"formId": "form1"}}}}
```

#### `TextField`

```json
{"id": "tf1", "component": "TextField", "label": {"path": "/data/tfLabel"}, "value": {"path": "/data/tfValue"}, "variant": "shortText|longText|number|obscured"}
```

#### `CheckBox`

```json
{"id": "cb1", "component": "CheckBox", "label": {"path": "/data/cbLabel"}, "value": {"path": "/data/cbChecked"}}
```

#### `ChoicePicker`

```json
{"id": "cp1", "component": "ChoicePicker", "label": {"path": "/data/cpLabel"}, "variant": "mutuallyExclusive|multipleSelection", "options": [{"label": {"path": "/data/opt1"}, "value": "v1"}, {"label": {"path": "/data/opt2"}, "value": "v2"}], "value": {"path": "/data/cpValue"}}
```

#### `Slider`

```json
{"id": "sl1", "component": "Slider", "label": {"path": "/data/slLabel"}, "min": 0, "max": 100, "value": {"path": "/data/slValue"}}
```

#### `DateTimeInput`

```json
{"id": "dt1", "component": "DateTimeInput", "label": {"path": "/data/dtLabel"}, "value": {"path": "/data/dtValue"}, "enableDate": true, "enableTime": true}
```

### Chart Components

#### `Chart`

Unified chart component. Use `chartType` to select visualization type.

```json
{"id": "chart1", "component": "Chart", "chartType": "bar", "title": {"path": "/data/chartTitle"}, "data": {"path": "/data/chartData"}}
```

`chartType` enum: `"bar"` | `"line"` | `"donut"` | `"bar_grouped"`

`data` structure (bar / line / bar_grouped):

```json
{"xAxis": ["Label1", "Label2"], "yAxis": ["Low", "Mid", "High"], "series": [{"name": "Series Name", "data": [{"value": 1.5, "label": "Display Text"}]}]}
```

`data` structure (donut):

```json
{"series": [{"name": "", "data": [{"label": "Category A", "value": 30}, {"label": "Category B", "value": 70}]}]}
```

#### `Table`

```json
{"id": "tb1", "component": "Table", "columns": {"path": "/data/columns"}, "rows": {"path": "/data/rows"}}
```

`dataModel` example:

```json
{"columns": ["Name", "Age", "City"], "rows": [["Alice", "25", "New York"], ["Bob", "30", "London"]]}
```

## Styles

### Common Styles

Common styles available on all atomic components:

```json
"styles": {
  "width": "100px",
  "height": "100px",
  "padding": "20px 20px 20px 20px",
  "padding-inline-start": "10px",
  "padding-left": "10px",
  "padding-inline-end": "10px",
  "padding-right": "10px",
  "padding-block-start": "10px",
  "padding-top": "10px",
  "padding-block-end": "10px",
  "padding-bottom": "10px",
  "margin": "0px 0px 32px 0px",
  "margin-inline-start": "10px",
  "margin-left": "10px",
  "margin-inline-end": "10px",
  "margin-right": "10px",
  "margin-block-start": "10px",
  "margin-top": "10px",
  "margin-block-end": "10px",
  "margin-bottom": "10px",
  "background-color": "#FFFFFF",
  "border-radius": "32px",
  "border-color": "#E0E0E0",
  "border-style": "solid",
  "border-width": "1px",
  "opacity": 0.8,
  "overflow": "hidden|visible",
  "display": "none",
  "visibility": "visible|hidden",
  "flex-grow": 1,
  "flex-shrink": 1,
  "flex-wrap": "nowrap|wrap|wrap-reverse",
  "justify-content": "flex-start|flex-end|center|space-between|space-around|space-evenly",
  "align-items": "flex-start|flex-end|center|stretch|baseline",
  "align-content": "flex-start|flex-end|center|space-between|space-around|stretch",
  "align-self": "auto|flex-start|flex-end|center|stretch|baseline",
  "aspect-ratio": "16 / 9",
  "filter": "drop-shadow(0px 4px 16px rgba(0, 0, 0, 0.08))"
}
```

Common style constraints:

- All size properties support `px` only
- `padding` / `margin` shorthand must use four values
- `background-color` supports `#RRGGBB`, `#RRGGBBAA`, `rgb(...)`, `rgba(...)`
- `border-style` supports only `"solid"`
- `justify-content` supports only `flex-start|flex-end|center|space-between|space-around|space-evenly`
- `align-items` supports only `flex-start|flex-end|center|stretch|baseline`
- `flex-wrap` supports only `nowrap|wrap|wrap-reverse`
- `align-content` supports only `flex-start|flex-end|center|space-between|space-around|stretch`
- `align-self` supports only `auto|flex-start|flex-end|center|stretch|baseline`
- `overflow` supports only `hidden|visible`
- `display` supports only `"none"`
- `visibility` supports only `visible|hidden`
- `filter` supports only `drop-shadow(...)`
- `flex-basis`, `gap`, `box-shadow`, `position`, `z-index` are not supported

### Text Styles

Text styles available on `Text` components only:

```json
"styles": {
  "color": "#0E142B",
  "font-size": "16px",
  "font-weight": "normal|bold",
  "font-family": "sans-serif",
  "line-height": 1.3,
  "text-align": "left top|left|left center|left bottom|center top|center|center center|center bottom|right top|right|right center|right bottom",
  "line-clamp": 2,
  "text-overflow": "clip|head|middle|ellipsis",
  "text-decoration": "underline solid #FF0000",
  "text-decoration-line": "underline|line-through",
  "text-decoration-style": "solid|dashed",
  "text-decoration-color": "#FF0000",
  "text-decoration-thickness": "2px"
}
```

Text style constraints:

- `color` supports only `#RRGGBB` or `#RRGGBBAA`
- `font-weight` supports only `normal|bold`
- `line-height` supports a multiplier or px; prefer multiplier for multi-line wrapping
- `text-align` supports only `left top|left|left center|left bottom|center top|center|center center|center bottom|right top|right|right center|right bottom`
- `text-overflow` supports only `clip|head|middle|ellipsis`, and requires `line-clamp`
- `text-decoration-line` supports only `underline|line-through`
- `text-decoration-style` supports only `solid|dashed`
- Text styles must not be applied to non-`Text` components

## Unified Font Scale

Font sizes must strictly use the following final output values:

- Global heading: `40px`
- Section title / secondary heading: `36px`
- Main content / secondary content / supporting content / table header / table data: `32px`
- Small text within cards: `28px`
- Tags / non-tag small labels / pills / footnotes: `24px`

POI scenario specific:

- POI name: `32px`
- Non-label text: `28px`
- Labels/tags: `24px`

Usage guidance:

- `h1` commonly uses `40px`
- `h2` / `h3` / section titles commonly use `36px`
- Normal body text, descriptions, table content start at `32px`
- Weaker supporting info commonly uses `28px`
- Tags, badges, pills, footnotes should converge to `24px`
