# Decision record template

Copy this into the [decision records](index.md) as a new numbered section
(or into its own page under `docs-site/decisions/`, added to the `nav` in
`mkdocs.yml`), and add a row to the table at the top.

```markdown
## N. The decision, as a short statement

**Status.** Proposed | Accepted | Superseded by N | Withdrawn
**Date.** YYYY-MM-DD, with the commits that carried it out

**Context.** What forced a choice: the problem, the measurements, the
constraints (platforms, budgets, licences, deadlines).

**Options.**

- **Option A**: what it is; what it costs; what it gives.
- **Option B**: ...

**Decision.** What was chosen, in one or two sentences, and the reason
that decided it.

**Consequences.** What follows, good and bad: what becomes easy, what
becomes hard, what must now be kept true (tests, budgets, checks), and
what would make the choice worth revisiting.

**See also.** The engine or feature pages that describe the code.
```

Tips:

- Write the record when the decision is made, and link it from the commit
  message; the context is hardest to recover later.
- Quote measurements (times, allocation counts, sizes) with the machine
  and build they came from.
- Do not edit an accepted record to reverse it: add a new one that
  supersedes it, and change the old one's status.
