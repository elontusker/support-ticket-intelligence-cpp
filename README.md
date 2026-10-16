# Support Ticket Intelligence

A command-line C++17 project that analyzes a customer-support ticket dataset and searches for previously reported issues with similar subject lines.

## Features

- Reads ticket records from a CSV file, including quoted fields.
- Reports ticket counts and first-response time averages and medians overall, by queue, and by priority.
- Counts common words in ticket subjects.
- Ranks tickets similar to a user-entered subject using TF-IDF and cosine similarity.
- Normalizes a small set of related words (for example, `access` and `log in`) before searching.
- Summarizes response times by queue for similar tickets scoring at least 50%, then reports the queue with the lowest median in that subset.

## Requirements

- A C++17 compiler. On Windows, GCC from the MSYS2 UCRT64 toolchain works.
- PowerShell or another terminal.

## Build and run on Windows

Open PowerShell in the project folder and run:

```powershell
g++ -std=c++17 .\main.cpp -o .\app.exe
.\app.exe
```

If PowerShell cannot find `g++`, add the MSYS2 UCRT64 tools folder to the current terminal's `PATH` (adjust the location if MSYS2 is installed elsewhere):

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
```

When prompted, enter a ticket subject, such as:

```text
I cannot access my account after changing my password
```

The program expects the CSV at `data/support-tickets.csv`, relative to the project folder. The included CSV is provided for convenience.

## How the search works

The program tokenizes subjects into lowercase words, removes common filler words, and maps a few hand-written synonyms to canonical terms. It then builds TF-IDF vectors and compares them with cosine similarity. This is a lightweight lexical search, not a language model: the synonym list is small, and the similarity score measures word-pattern overlap rather than whether two tickets truly have the same meaning.

The 50% cutoff used in the queue summary is a simple project setting. The queue insight describes historical patterns in the included dataset; it is not a recommendation or guarantee for routing a real support ticket.

## Dataset

The sample contains 4,000 synthetic customer support tickets from [Datanemics' Customer Support Tickets dataset](https://datanemics.com/datasets/support-tickets/). It is dedicated to the public domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

## Project structure

```text
support-ticket-intelligence/
├── main.cpp
├── data/
│   └── support-tickets.csv
├── .gitignore
└── README.md
```
