---
name: publish-effect-completed-work
description: Publish completed Expedition_158 EffectEditor or effect-owner documents to the Notion "이펙트 담당 완료 작업 DB". Use when the user says a completed local document should be registered in Notion, asks to match existing DB page style, or asks to judge related documents strictly from existing Notion DB pages.
---

# Publish Effect Completed Work

## Workflow

1. Identify the completed local document named by the user and read it before drafting.
2. Use the Notion plugin or connector, preferably the Notion knowledge-capture workflow, to access `이펙트 담당 완료 작업 DB`.
3. Inspect representative existing pages in that DB before creating a new page.
4. Draft a team-facing completion record that follows the DB's existing property and body style.
5. For `관련 문서`, use only existing Notion DB page titles that are directly defensible as prerequisites, follow-ups, or tightly related completed records.
6. Do not use local repo paths, filenames, English slugs, or weak topic similarity as related documents.
7. Create a fresh page in the DB, then fetch/read it back and verify the title, properties, body order, and `관련 문서` wording.

## References

Read `references/notion-completed-db.md` before drafting or publishing. It contains the stable DB id, property names, page body order, and related-document rule for this project.
