# Tools Used

## AI Assistants

| Tool and version | Used By | Used for | Files affected | What we changed or verified |
|---|---|---|---|---|
| ChatGPT; exact model/version not recorded | Everyone | Discussing the common interface, simulator, integration errors and dashboard | `app/ITriageQueue.h`, `app/main.cpp` `dashboard/` | Reviewed and adapted suggestions for the project. Compilation and Oracle testing were performed during integration |

AI assistance was used to generate explanations, draft code and
documentation, and help diagnose integration issues. The team is
responsible for understanding and verifying the submitted work.

## Development and Testing Tools

| Tool | Used for |
|---|---|
| Visual Studio Code | Editing, terminal access, and debugging |
| GCC / g++ | Compiling the C++ project |
| GNU Make / mingw32-make | Running build and test targets |
| Python | Running experiment and input-generation scripts |
| Node.js | Running the local dashboard server |
| Git and GitHub | Version control, branches, and collaboration |
| Google Slides | Preparing the presentation |

Compiler and tool versions used for the final experiments are recorded
in the experimental setup in `REPORT.md`, where available.

## Representative Prompts

- “How should the common interface connect the simulator, Structure A,
  Structure B, and the Oracle?”
- “Explain why linear aging can use a static priority key.”
- “How should step-wise aging update patient priority?”
- “Help diagnose the compilation and integration errors.”
- “How can the simulation results be shown on a dashboard?”
- “Prepare a project report following the submission guidelines.”

## Review and Verification

Suggestions were reviewed and adapted during development. The team
compiled the project and ran Oracle tests during integration. The final
report should distinguish tests actually run on the final commit from
tests that are incomplete or still need verification.