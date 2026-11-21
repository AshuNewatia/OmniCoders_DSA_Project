const http = require("http");
const fs = require("fs");
const path = require("path");
const { spawn } = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const RESULTS = path.join(ROOT, "results");
const PORT = 3000;

let simulationRunning = false;

const structures = {
  a: {
    file: "src/structA/structure_A.cpp",
    className: "Min_Heap",
  },

  b: {
    file: "src/structB/StructureB.cpp",
    className: "LazyTriageQueue",
  },

  oracle: {
    file: null,
    className: "OracleQueue",
  },
};

function sendJson(response, status, data) {
  response.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });

  response.end(JSON.stringify(data));
}

function runProcess(command, args) {
  return new Promise((resolve, reject) => {
    const process = spawn(command, args, {
      cwd: ROOT,
      windowsHide: true,
    });

    let stdout = "";
    let stderr = "";

    const timeout = setTimeout(() => {
      process.kill();
      reject(new Error("The process took too long and was stopped."));
    }, 120000);

    process.stdout.on("data", (data) => {
      stdout += data.toString();
    });

    process.stderr.on("data", (data) => {
      stderr += data.toString();
    });

    process.on("error", (error) => {
      clearTimeout(timeout);
      reject(error);
    });

    process.on("close", (code) => {
      clearTimeout(timeout);

      if (code === 0) {
        resolve(stdout);
      } else {
        reject(
          new Error(
            stderr || stdout || `Process exited with code ${code}.`
          )
        );
      }
    });
  });
}

function readEvents(csvPath) {
  const csv = fs.readFileSync(csvPath, "utf8");
  const lines = csv.trim().split(/\r?\n/);

  const header = lines.shift().replace(/^\uFEFF/, "");

  const expectedHeader =
    "minute,event,patient_id,wait_time,severity,priority_key";

  if (header !== expectedHeader) {
    throw new Error(
      "Unexpected CSV format. Check the columns in your simulator output."
    );
  }

  return lines
    .filter(Boolean)
    .map((line) => {
      const [
        minute,
        event,
        id,
        wait,
        severity,
        key,
      ] = line.split(",");

      return {
        minute: Number(minute),
        event,
        id: Number(id),
        wait: Number(wait),
        severity: Number(severity),
        key: Number(key),
      };
    });
}

async function runSimulation(request, response, url) {
  const structure = url.searchParams.get("structure");
  const policy = url.searchParams.get("policy");
  const seed = url.searchParams.get("seed");

  const validStructure =
    Object.prototype.hasOwnProperty.call(structures, structure);

  const validPolicy =
    policy === "linear" || policy === "stepwise";

  const validSeed = /^\d{1,9}$/.test(seed || "");

  if (!validStructure || !validPolicy || !validSeed) {
    return sendJson(response, 400, {
      error: "Please select a valid structure, policy, and seed.",
    });
  }

  if (simulationRunning) {
    return sendJson(response, 409, {
      error: "A simulation is already running. Please wait.",
    });
  }

  simulationRunning = true;

  const wrapperFile = path.join(
    __dirname,
    "runner.generated.cpp"
  );

  const executableName =
    process.platform === "win32"
      ? "runner.generated.exe"
      : "runner.generated";

  const executable = path.join(__dirname, executableName);

  const csvFile = path.join(
    RESULTS,
    "dashboard_latest.csv"
  );

  try {
    fs.mkdirSync(RESULTS, { recursive: true });

    const selected = structures[structure];

    const structureInclude = selected.file
      ? `#include "../${selected.file}"\n`
      : "";

    const agingPolicy =
      policy === "linear"
        ? "AgingPolicy::LINEAR"
        : "AgingPolicy::STEP_WISE";

    const wrapperCode = `
${structureInclude}

#define main original_simulator_main
#include "../app/main.cpp"
#undef main

int main() {
    ${selected.className} queue;

    simulation(
        &queue,
        ${agingPolicy},
        ${seed}ULL,
        "results/dashboard_latest.csv"
    );

    return 0;
}
`;

    fs.writeFileSync(wrapperFile, wrapperCode);

    if (fs.existsSync(csvFile)) {
      fs.unlinkSync(csvFile);
    }

    await runProcess("g++", [
      "-std=c++17",
      "-O2",
      wrapperFile,
      path.join(ROOT, "oracle", "OracleQueue.cpp"),
      "-o",
      executable,
    ]);

    await runProcess(executable, []);

    if (!fs.existsSync(csvFile)) {
      throw new Error(
        "The simulation did not create results/dashboard_latest.csv."
      );
    }

    const events = readEvents(csvFile);

    return sendJson(response, 200, {
      structure,
      policy,
      seed,
      events,
    });
  } catch (error) {
    return sendJson(response, 500, {
      error: error.message,
    });
  } finally {
    simulationRunning = false;

    try {
      fs.unlinkSync(wrapperFile);
    } catch (_) {
      // The temporary file may not have been created.
    }
  }
}

const server = http.createServer(async (request, response) => {
  const url = new URL(
    request.url,
    `http://localhost:${PORT}`
  );

  if (request.method === "GET" && url.pathname === "/") {
    const htmlFile = path.join(
      __dirname,
      "public",
      "index.html"
    );

    fs.readFile(htmlFile, (error, data) => {
      if (error) {
        response.writeHead(500);
        response.end("Could not load the dashboard.");
        return;
      }

      response.writeHead(200, {
        "Content-Type": "text/html; charset=utf-8",
      });

      response.end(data);
    });

    return;
  }

  if (
    request.method === "POST" &&
    url.pathname === "/api/run"
  ) {
    return runSimulation(request, response, url);
  }

  return sendJson(response, 404, {
    error: "Route not found.",
  });
});

server.listen(PORT, "127.0.0.1", () => {
  console.log(`Dashboard running at http://localhost:${PORT}`);
});