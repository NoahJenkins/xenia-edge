import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import { Evaluator, Lexer, Parser } from '@actions/expressions';
import { reviver } from '@actions/expressions/data/reviver';
import { parse } from 'yaml';

const root = fileURLToPath(new URL('../../', import.meta.url));
const load = path => parse(readFileSync(`${root}${path}`, 'utf8'));
const build = load('.github/workflows/build.yml');
const ci = load('.github/workflows/CI.yml');
const repository = 'NoahJenkins/xenia-edge';
const runners = {
  lint: ['ubuntu-24.04', 'optiplex-ci-ubuntu24'],
  'build-linux': ['ubuntu-22.04', 'optiplex-ci-ubuntu22'],
};

// Use GitHub's parser/evaluator rather than reimplementing routing in JavaScript.
function evaluate(expression, context) {
  const source = expression.trim().replace(/^\$\{\{\s*|\s*\}\}$/g, '');
  const tokens = new Lexer(source).lex().tokens;
  const ast = new Parser(tokens, Object.keys(context), []).parse();
  return new Evaluator(ast, JSON.parse(JSON.stringify(context), reviver)).evaluate();
}

function caller(eventName, head = repository, actor = 'contributor') {
  // workflow_call retains the triggering caller event; it does not replace it
  // with an event_name of workflow_call.
  return {
    repository,
    event_name: eventName,
    actor,
    ref: eventName === 'pull_request' ? 'refs/pull/1/merge' : 'refs/heads/edge',
    event: eventName === 'pull_request'
      ? { pull_request: { draft: false, head: { repo: head ? { full_name: head } : null } } }
      : {},
  };
}

const events = [
  ['push', caller('push'), true],
  ['manual', caller('workflow_dispatch'), true],
  ['same repository PR', caller('pull_request'), true],
  ['same repository Dependabot PR', caller('pull_request', repository, 'dependabot[bot]'), true],
  ['fork PR', caller('pull_request', 'outside/xenia-edge'), false],
  ['Dependabot fork PR', caller('pull_request', 'outside/xenia-edge', 'dependabot[bot]'), false],
  ['owner-authored fork PR', caller('pull_request', 'outside/xenia-edge', 'NoahJenkins'), false],
  ['missing PR head repository', caller('pull_request', null), false],
];

for (const [name, github, trusted] of events) {
  for (const value of [undefined, '', 'false', 'true', '1', 'yes', ' true', 'true ', '"true"']) {
    test(`${name}: opt-in ${JSON.stringify(value) ?? 'missing'}`, () => {
      const vars = value === undefined ? {} : { OPTIPLEX_CI_ENABLED: value };
      assert.equal(evaluate(ci.jobs.build.if, { github }).value, true);
      for (const [job, [hosted, dell]] of Object.entries(runners)) {
        assert.equal(
          evaluate(build.jobs[job]['runs-on'], { github, vars }).value,
          value === 'true' && trusted ? dell : hosted,
          job,
        );
      }
    });
  }
}

for (const value of ['TRUE', 'True', 'tRuE']) {
  test(`non-JSON opt-in ${value} fails closed for trusted callers`, () => {
    for (const job of Object.keys(runners)) {
      assert.throws(() => evaluate(build.jobs[job]['runs-on'], {
        github: caller('push'), vars: { OPTIPLEX_CI_ENABLED: value },
      }), /parsing JSON/);
    }
  });
  test(`fork uses hosted even with invalid opt-in ${value}`, () => {
    for (const [job, [hosted]] of Object.entries(runners)) {
      assert.equal(evaluate(build.jobs[job]['runs-on'], {
        github: caller('pull_request', 'outside/xenia-edge'),
        vars: { OPTIPLEX_CI_ENABLED: value },
      }).value, hosted);
    }
  });
}

test('only the two Linux selectors change relative to the reviewed base', () => {
  const baseline = process.env.CI_BASE_REF || '9d8210b32ddd3f0c8ab7d5df18798ec9e513aa65';
  const original = path => parse(execFileSync('git', ['show', `${baseline}:${path}`], {
    cwd: root, encoding: 'utf8',
  }));
  const normalized = structuredClone(build);
  const before = original('.github/workflows/build.yml');
  for (const job of Object.keys(runners)) {
    normalized.jobs[job]['runs-on'] = before.jobs[job]['runs-on'];
  }
  // Covers job names, dependencies, steps, action pins, artifact paths/names,
  // permissions, triggers, timeouts and cancellation without copying YAML.
  assert.deepEqual(normalized, before);
  assert.deepEqual(ci, original('.github/workflows/CI.yml'));
  assert.equal(ci.jobs.build.uses, './.github/workflows/build.yml');
  assert.deepEqual(ci.jobs.build.permissions, { contents: 'read', actions: 'write' });
  assert.equal(build.jobs['build-macos-arm64']['runs-on'], 'macos-26');
  assert.equal(build.jobs['build-macos-x86_64']['runs-on'], 'macos-26');
  assert.equal(build.jobs['build-windows']['runs-on'], 'windows-2025');
  assert.equal(ci.jobs.release['runs-on'], 'macos-26');
});

test('draft PR suppression and upstream-only release publication are preserved', () => {
  const github = caller('pull_request');
  github.event.pull_request.draft = true;
  assert.equal(evaluate(ci.jobs.build.if, { github }).value, false);
  for (const repo of [repository, 'has207/xenia-edge']) {
    for (const event of ['push', 'pull_request', 'workflow_dispatch']) {
      for (const ref of ['refs/heads/edge', 'refs/heads/other', 'refs/tags/v1']) {
        assert.equal(evaluate(ci.jobs.release.if, {
          github: { ...caller(event), repository: repo, ref },
        }).value, repo === 'has207/xenia-edge' && event === 'push' && ref === 'refs/heads/edge');
      }
    }
  }
});

test('caller downloads every reusable build artifact by its original name', () => {
  const uploaded = Object.values(build.jobs).flatMap(job => job.steps)
    .filter(step => step.uses?.startsWith('actions/upload-artifact@'))
    .map(step => step.with.name).sort();
  const downloaded = ci.jobs.release.steps
    .filter(step => step.uses?.startsWith('actions/download-artifact@'))
    .map(step => step.with.name).sort();
  assert.deepEqual(downloaded, uploaded);
  assert.ok(downloaded.includes('xenia_edge_linux'));
});
