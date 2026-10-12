# Exporting Coverity Scan defects for offline triage

`coverity_export.py` pulls LAMMPS-GUI's static-analysis results off the free /
open-source [Coverity Scan](https://scan.coverity.com/) service into local JSON
so the defects can be triaged offline (matched to source, sorted into
real-bug-fix vs. intentional / false-positive-to-dismiss).

This is a *manual* helper for periodic triage; it complements, and does not
replace, the daily upload workflow in `.github/workflows/coverity-scan.yml`. It
is not part of the CTest suite and needs no build -- it is a standalone script.
It drives the same *undocumented* internal JSON endpoints the "View Defects" web
UI calls, reusing your authenticated browser session, so it can break without
notice if Coverity changes those endpoints.

Adapted from the LAMMPS distribution (`tools/coverity`, PR
[lammps/lammps#5057](https://github.com/lammps/lammps/pull/5057)); GPL, same as
the rest of LAMMPS-GUI.

## Project coordinates (already the built-in defaults)

| Field | Value |
|---|---|
| Coverity project | `akohlmey/lammps-gui` |
| `projectId` | `16333` |
| all-defects `viewId` | `67542` |
| server shard host | `scan8.scan.coverity.com` |

Because these are baked in as defaults, the `defects`/`traces` commands below
run with no `-p` / `-v` / `--*-url` flags. Override them only if the project id,
the view id, or the **server shard** changes (see the shard note below).

## Requirements

```bash
pip install requests
```

## Getting the authentication cookie (do this first)

The endpoints require your logged-in session cookie. It is **not** an API token;
it is the browser session, and it **expires** (the tool aborts with
"Authentication failed ... re-capture it" when it does -- just repeat these
steps). It is a live credential: keep it out of the repo and delete the file
when done.

1. Log in to <https://scan.coverity.com/> and open the LAMMPS-GUI project's
   **View Defects** page.
2. Open the browser **Developer Tools -> Network** tab (F12) and reload the
   defects view so requests appear.
3. Find a request to `table.json` (the grid) or `source.json` (one defect).
   Right-click it -> **Copy -> Copy as cURL**.
4. From the copied cURL take the value of the `-b` / `cookie:` header. On the
   open-source tier it looks like
   `COVJSESSIONID-build=...; XSRF-TOKEN=...; isAuthenticated=true; ...`. Put it in
   a file outside the repo and point the tool at it:

   ```bash
   COOKIE_FILE="$(mktemp)"        # NOT inside the git tree
   printf '%s' 'COVJSESSIONID-build=...; XSRF-TOKEN=...; isAuthenticated=true; ...' > "$COOKIE_FILE"
   export COVERITY_COOKIE="$(cat "$COOKIE_FILE")"
   ```

   (`--cookie`/`--cookie-file` work too.) The `XSRF-TOKEN` is extracted from the
   cookie and echoed as the `X-XSRF-TOKEN` header automatically -- just include
   the whole cookie string. **Never commit the cookie or the exported JSON.**

> **Server shard note.** Coverity Scan shards projects across `scanN` hosts.
> LAMMPS-GUI is on **scan8**, which is the built-in default. If the "Copy as
> cURL" URL ever shows a different `scanN.scan.coverity.com`, the cookie is only
> valid for that shard -- pointing the tool at the wrong host fails with a
> misleading "cookie expired" error. In that case pass the right host:
> `--views-url https://scanN.scan.coverity.com/views/table.json`,
> `--reports-url .../reports/table.json`, and for traces the matching
> `--url-template` (below).

## Step 1 -- export the defect grid (all pages)

```bash
python coverity_export.py defects -o exported_defects.json
```

One command collects the **whole** set (it walks the pages via a stateful POST
page-setter + GET reader on one session). The output records `server_total` and
`count`; a mismatch is flagged.

## Step 2 -- export per-defect event traces

```bash
python coverity_export.py traces --cids-from exported_defects.json -o traces.json
```

This reads the `fileInstanceId` / `defectInstanceId` for each CID out of the
grid export and fetches each defect's numbered event trace (file + line). Add
`--cids '647309 647322'` to limit it to specific CIDs. Each
`defect_traces/cid_<CID>.json` is the raw payload; `traces.json` is the
flattened per-CID event list. Re-runs reuse cached files; `--refresh` forces a
re-fetch.

If the shard is not the default, add the matching trace template:

```bash
python coverity_export.py traces --cids-from exported_defects.json -o traces.json \
    --url-template 'https://scanN.scan.coverity.com/sourcebrowser/source.json?projectId={projectId}&fileInstanceId={fileInstanceId}&defectInstanceId={defectInstanceId}&mergedDefectId={mergedDefectId}&fileStart=&fileEnd='
```

## Reading the output

The grid rows carry the useful fields in `displayType`, `displayImpact`,
`displayFile`, `displayFunction`, and `classification`/`status` -- note that the
plain `checker` and `lineNumber` columns come back blank, so use `displayType`
and the trace events for the real defect line.

## Offline check

```bash
python coverity_export.py self-test
```

exercises the POST paging/dedup and trace-extraction/auth/XSRF logic with no
network or cookie.
