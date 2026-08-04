# Blog Articles

Source of truth for the blog articles rendered on the AGenUI website at `/logs`.

The website fetches these files at runtime from `raw.githubusercontent.com`. It does
not keep a local copy, so **this directory is the only place the articles live**.

## Layout

```
docs/articles/
├── index.json                 # manifest: the list + all article metadata
├── <slug>.zh-CN.md            # article body
└── README.md                  # this file
```

## Publishing an article

Every article is **two** edits. Missing either one makes the article invisible.

1. Add `docs/articles/<slug>.zh-CN.md`
2. Add an entry to the `articles` array in `docs/articles/index.json`

The website derives the markdown filename from `slug` (`<slug>.zh-CN.md`), so the two
must agree exactly.

## `index.json` schema

```json
{
  "version": 1,
  "articles": [
    {
      "slug": "my-article",
      "title": "...",
      "excerpt": "...",
      "author": "AGenUI Team",
      "date": "2026-07-27",
      "category": "技术解读",
      "tags": ["A2UI"]
    }
  ]
}
```

| Field | Required | Notes |
| --- | --- | --- |
| `slug` | yes | Must match `<slug>.zh-CN.md`. Also becomes the URL: `/logs/<slug>` |
| `title` | yes | Rendered as the page `<h1>` |
| `excerpt` | yes | Card summary on the list page, clamped to 2 lines |
| `author` | yes | |
| `date` | yes | Strictly `YYYY-MM-DD`. The list is sorted by this field, descending — the array order in this file is ignored. `2026-7-1` will sort incorrectly |
| `category` | yes | Prefer one of `技术解读` / `实践分享` / `版本发布` / `社区动态`; other values are grouped at the end of the category filter |
| `tags` | yes | Array of strings, may be empty |
| `readTime` | no | Defaults to empty |
| `featured` | no | Defaults to `false`; `true` shows a "featured" badge |

## Markdown rules

- **Keep the leading `# Heading`.** It makes the file readable on its own here on
  GitHub. The website strips it before rendering, because the `<h1>` is already
  rendered from `index.json`'s `title`.
- **Do not add a language-switch line.** `docs/API.zh-CN.md` and
  `docs/QuickStart.zh-CN.md` start with a `[English](...) | 中文` line because they
  are bilingual; articles are not, and the website does not strip that line.
- **All links and images must be absolute HTTPS URLs.** The website renders markdown
  verbatim, so a relative path like `./images/x.png` resolves against the website
  origin and 404s.
- Articles are Chinese-only, hence the `.zh-CN.md` suffix (see
  `agent-context/rules/english-only.md`). If an article is ever translated, add
  `<slug>.md` alongside it — the website will need a change to pick it up.

## Propagation

The website reads the **public GitHub mirror**, not this GitLab repository. Pushing
here is not enough; the change must reach `github.com/AGenUI/AGenUI` on `main`.

`raw.githubusercontent.com` serves `cache-control: max-age=300`, so allow up to
~5 minutes after the mirror updates before the change is visible.

## Verifying

```bash
BASE=https://raw.githubusercontent.com/AGenUI/AGenUI/refs/heads/main/docs/articles
for p in index.json my-article.zh-CN.md; do
  echo "$(curl -s -o /dev/null -w '%{http_code}' "$BASE/$p")  $p"
done
```

Both must return `200`. If `index.json` is unreachable the website shows a "failed to
load" state with a retry button — it deliberately does not show an empty blog, so a
broken mirror is visible rather than silent.
