# FreeSpace command and input errors

FreeSpace retains human-readable stderr diagnostics; the manifest declares `errorContract.type=none`. Extract returns 0 on success, 1 for a diagnosed exception and 2 for an unknown exception. Solve returns 0 or 1. Post returns 0, 1 for a diagnosed failure and 2 for invalid CLI syntax. An invalid platform invocation does not imply usable artifacts.

| Diagnostic | Meaning and recovery |
| --- | --- |
| `field is not supported by Free Space` | Remove `Taylor`, `MNA_WEIGHT`, `SOLVER_MEMORY_MB` and `SOLVER_THREADS`; zero is also rejected. Use supported CLI thread limits. |
| `SAVE_BIN/SAVE_TXT are not supported in standard mode` | Remove both obsolete format switches, including zero. Extraction always produces model.peec. Legacy modular values warn as deprecated. |
| `Unsupported FreeSpace model.peec` / `Package metadata/topology digest mismatch` | The new container is invalid/corrupted or is not FreeSpace PEECFS02. Re-extract; no historical-file fallback is allowed. |
| `Unknown FreeSpace setting` | Check flat field names against `schemas/set_em.schema.json`; CLI controls do not belong in JSON. |
| `meshSha256 does not match meshFile` | Source belongs to a different mesh. Regenerate the source against the actual original mesh; do not substitute dense triangle indices. |
| `not a PEC triangle tag` | A source endpoint does not identify a real original Tri3 tag. |
| `surface C conductors` / `volume/high-order physics are unsupported` | Use a first-order PEC surface input or an engine that supports the required physics. |
| `Triangle material label is missing` | Every Tri3 physical label must resolve to a surface C material in MATDB schema 3. |
| `Standard input is missing or stale` | One of the four original files changed or disappeared. Extract and solve again. |
| `Private standard input is missing or stale` | A derived input changed. Re-extract from the original standard root. |
| `output is incomplete` / `stale or corrupted` | A stage failed, was interrupted, or its output changed. Rerun the named stage and its dependents. |
| `provider directory` / `symlink or junction` | Runtime/output paths must remain under their provider directories and cannot traverse reparse points. |

Each started standard stage writes a best-effort v1 `stage_receipt.json` in its own output directory, including original input fingerprints, parameters, exit code and successful output hashes. Missing input entries are explicit. Receipt/log write failure prints a warning and preserves the computational exit code. Mandatory private stage states and incomplete markers govern artifact reuse independently of receipts.
