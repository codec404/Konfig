# Branching Strategy

## Overview

This project follows a **Dev → Master** branching strategy where:

- `dev` is the primary development branch
- `master` is the production-ready branch
- Feature branches merge into `dev`
- `dev` merges into `master` via automated PRs (D2M - Dev to Master)

## Branch Structure

```text
┌─────────────┐
│   master    │  ← Production branch (protected)
└──────▲──────┘
       │
       │  D2M (Auto PR)
       │
┌──────┴──────┐
│     dev     │  ← Main development branch (default)
└──────▲──────┘
       │
       │  PRs
       │
┌──────┴──────┐
│  feature/*  │  ← Feature branches
│   bugfix/*  │  ← Bug fix branches
│  hotfix/*   │  ← Hotfix branches
└─────────────┘
```

## Branches

### `master` (Production)

- **Purpose:** Stable, production-ready code
- **Protection:** Highly protected, requires PR reviews
- **Merges:** Only accepts merges from `dev` (via D2M PRs) or emergency hotfixes
- **CI/CD:** Triggers production deployments
- **Never commit directly to this branch**

### `dev` (Development)

- **Purpose:** Integration branch for all development work
- **Default Branch:** ✅ All PRs target this branch by default
- **Protection:** Protected, requires PR reviews
- **Merges:** Accepts merges from feature/bugfix branches
- **CI/CD:** Triggers staging deployments and auto-generates D2M PRs
- **Never commit directly to this branch**

### Feature Branches

- **Naming:** `feature/<ticket-id>-<short-description>`
- **Example:** `feature/CONF-123-add-redis-cache`
- **Branch from:** `dev`
- **Merge into:** `dev`
- **Lifespan:** Delete after merge

### Bugfix Branches

- **Naming:** `bugfix/<ticket-id>-<short-description>`
- **Example:** `bugfix/CONF-456-fix-memory-leak`
- **Branch from:** `dev`
- **Merge into:** `dev`
- **Lifespan:** Delete after merge

### Hotfix Branches

- **Naming:** `hotfix/<ticket-id>-<short-description>`
- **Example:** `hotfix/CONF-789-critical-security-fix`
- **Branch from:** `master`
- **Merge into:** Both `master` AND `dev`
- **Lifespan:** Delete after merge
- **Use sparingly:** Only for critical production fixes

## Workflow

### 1. Feature Development (Standard Workflow)

```bash
# 1. Ensure you're on dev and up to date
git checkout dev
git pull origin dev

# 2. Create feature branch
git checkout -b feature/CONF-123-add-caching

# 3. Make changes and commit
git add .
git commit -m "Add Redis caching layer"

# 4. Push to remote
git push origin feature/CONF-123-add-caching

# 5. Create PR to dev (via GitHub UI)
# - Base: dev
# - Compare: feature/CONF-123-add-caching

# 6. After PR is approved and merged, delete feature branch
git checkout dev
git pull origin dev
git branch -d feature/CONF-123-add-caching
git push origin --delete feature/CONF-123-add-caching
```

### 2. Dev to Master (D2M) - Automated

```text
1. When a PR merges to `dev`, CI automatically:
   - Triggers the "Auto PR Dev to Master" workflow
   - Creates/updates a D2M PR from `dev` to `master`
   - Adds label: "auto-pr", "dev-to-master", "d2m"
   - Assigns reviewers

2. Review the D2M PR:
   - Verify all CI checks pass
   - Review the changes since last master merge
   - Ensure documentation is updated
   - Confirm production readiness

3. Merge the D2M PR to deploy to production
```

### 3. Hotfix (Emergency Production Fix)

```bash
# 1. Branch from master
git checkout master
git pull origin master
git checkout -b hotfix/CONF-999-urgent-fix

# 2. Make the fix
git add .
git commit -m "Fix critical production issue"

# 3. Push and create PR to master
git push origin hotfix/CONF-999-urgent-fix
# Create PR: base=master, compare=hotfix/CONF-999-urgent-fix

# 4. After merging to master, also merge to dev
git checkout dev
git pull origin dev
git merge master
git push origin dev

# 5. Delete hotfix branch
git branch -d hotfix/CONF-999-urgent-fix
git push origin --delete hotfix/CONF-999-urgent-fix
```

## Pull Request Guidelines

### When Creating a PR

1. **Always target `dev`** (unless it's a hotfix or D2M)
2. Fill out the PR template completely
3. Link related issues
4. Add appropriate labels
5. Request reviews from team members
6. Ensure CI checks pass

### PR Review Checklist

- [ ] Code follows project style guidelines
- [ ] All tests pass
- [ ] Documentation updated (if needed)
- [ ] No breaking changes (or properly documented)
- [ ] Commits are clean and well-described
- [ ] No merge conflicts

## GitHub Repository Settings

### Set `dev` as Default Branch

To ensure all PRs default to `dev`:

1. Go to **Settings** → **Branches**
2. Under "Default branch", click the switch icon
3. Select `dev`
4. Click "Update"

### Branch Protection Rules

#### For `master`:

```yaml
Required:
  - Require pull request reviews before merging (1+ approvals)
  - Require status checks to pass before merging
  - Require branches to be up to date before merging
  - Do not allow bypassing the above settings

Status Checks:
  - CI / summary
  - CI / lint-and-format
  - CI / proto-validation
  - CI / docker-compose-validation
```

#### For `dev`:

```yaml
Required:
  - Require pull request reviews before merging (1+ approvals)
  - Require status checks to pass before merging
  - Require branches to be up to date before merging

Status Checks:
  - CI / summary
  - CI / lint-and-format
  - CI / proto-validation
```

## CI/CD Integration

### CI Triggers

- Runs on all PRs to `dev` and `master`
- Runs on direct pushes to `dev` and `master` (discouraged)

### Auto-PR (D2M) Trigger

- Runs when code is pushed to `dev`
- Creates/updates PR from `dev` to `master`
- Includes recent commits and checklist

## Best Practices

1. **Never commit directly to `dev` or `master`**
   - Always use feature branches and PRs

2. **Keep feature branches short-lived**
   - Merge within 1-2 days to avoid conflicts

3. **Sync feature branches regularly**

   ```bash
   git checkout feature/my-feature
   git fetch origin
   git merge origin/dev
   ```

4. **Write descriptive commit messages**

   ```text
   Good: "Add Redis caching for user sessions"
   Bad:  "fix stuff"
   ```

5. **Review your own PR first**
   - Check the diff before requesting reviews
   - Ensure no debug code or secrets are committed

6. **Delete merged branches**
   - Keep repository clean
   - Use GitHub's auto-delete feature

## Troubleshooting

### "My PR is targeting the wrong branch"

1. In GitHub PR UI, click "Edit" next to the base branch
2. Select `dev` as the base branch
3. Save changes

### "I committed to dev by mistake"

```bash
# Move the commit to a new branch
git checkout dev
git branch feature/my-accidental-commit
git reset --hard origin/dev
git checkout feature/my-accidental-commit
git push origin feature/my-accidental-commit
```

### "D2M PR has conflicts"

```bash
# Update dev with master changes
git checkout dev
git pull origin dev
git merge origin/master
# Resolve conflicts
git add .
git commit -m "Merge master into dev to resolve conflicts"
git push origin dev
```

## Questions?

- Check the [COMMANDS.md](../COMMANDS.md) for make commands
- Check the [README.md](../README.md) for project overview
- Ask the team in the project chat

---

**Remember:** `dev` is your friend! Always branch from and merge to `dev` for regular development work.
