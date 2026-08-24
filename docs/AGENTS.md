# docs/AGENTS.md

## Scope

Documentation process for ADRs, architecture docs, context notes, research reports, and docs trackers in this repository.

## ADR Rules

- Location: `docs/adr/`
- Naming: `NNNN-short-title.md`
- ADRs are append-only and immutable; never rewrite an accepted ADR
- Required sections: Status, Context, Options Considered, Decision, Consequences
- Write an ADR when a change affects structure, dependencies, interfaces, non-functional requirements, workflow policy, or tool/vendor selection
- If a decision changes, create a new ADR that supersedes the earlier one

## Context Notes

- Location: `docs/context/`
- Naming: `YYYY-MM-DD-topic-name.md`
- Required sections: Summary, Findings, Open Questions
- Use context notes for research, planning, investigation, onboarding outcomes, debugging learnings, and implementation rationale
- Update `docs/context/index.md` when adding a new context note if the repo maintains an index
- When a context note is created alongside an ADR, add `<!-- Related ADR: [ADR NNNN](../adr/NNNN-short-title.md) -->` at the bottom

## Architecture Docs

- Location: `docs/architecture/`
- Update when execution flow, system design, integrations, runtime boundaries, or operational procedures change
- Keep architecture docs concise and current-state focused
- Include purpose, current state, key design decisions, and diagrams when they add clarity

## Research Reports

- Location: `docs/researchReports/`
- Naming: `YYYY-MM-DD-topic-name.md`
- Optional: use only for formal evaluations, comparative studies, or findings likely to be referenced repeatedly
- Preferred sections: Purpose, Methodology, Findings, Recommendations, References

## Task Tracker

- If the repo uses `docs/TODO.md`, treat it as the living tracker for follow-ups and docs debt
- Update the `Last Updated` line on every change
- Mark completed work immediately and capture newly discovered follow-ups before ending a task

## Threshold Rules

- Create or update a context note for non-trivial research, planning, debugging, or rationale-heavy work
- Create an ADR when a durable decision is made or an earlier one is superseded
- Update architecture docs when system boundaries, integrations, runtime flow, or operations materially change
- Skip new docs artifacts for straightforward local fixes unless they expose a reusable rule or important rationale
