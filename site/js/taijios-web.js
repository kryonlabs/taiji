(() => {
  "use strict";

  const v86Base = "assets/v86";
  const assetVersion = "20260826-oom";
  const versioned = (path) => `${path}?v=${assetVersion}`;
  const emulatorConfig = {
    wasm_path: versioned(`${v86Base}/v86.wasm`),
    memory_size: 256 * 1024 * 1024,
    vga_memory_size: 8 * 1024 * 1024,
    screen_container: document.getElementById("screen-container"),
    bios: { url: versioned(`${v86Base}/seabios.bin`) },
    vga_bios: { url: versioned(`${v86Base}/vgabios.bin`) },
    hda: {
      url: versioned("assets/taijios-web.raw"),
    },
    net_device: { type: "virtio" },
    disable_speaker: true,
    autostart: true,
  };

  const startButton = document.getElementById("start-emulator");
  const resetButton = document.getElementById("reset-emulator");
  const fullscreenButton = document.getElementById("fullscreen-emulator");
  const bootChoiceButtons = Array.from(document.querySelectorAll(".boot-choice"));
  const statusText = document.getElementById("emulator-status");
  const statusDot = document.getElementById("status-dot");
  const placeholder = document.getElementById("screen-placeholder");
  const screenContainer = document.getElementById("screen-container");
  let emulator = null;
  let v86LoadPromise = null;
  let autoBootQueued = false;

  function setStatus(message, kind) {
    statusText.textContent = message;
    statusDot.className = "status-dot";
    if (kind) {
      statusDot.classList.add(`is-${kind}`);
    }
  }

  function loadScript(src) {
    return new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = src;
      script.async = true;
      script.onload = resolve;
      script.onerror = () => reject(new Error(`Could not load ${src}`));
      document.head.appendChild(script);
    });
  }

  async function loadV86() {
    if (window.V86 || window.V86Starter) {
      return;
    }
    if (!v86LoadPromise) {
      v86LoadPromise = loadScript(versioned(`${v86Base}/libv86.js`));
    }
    await v86LoadPromise;
    if (!window.V86 && !window.V86Starter) {
      throw new Error("v86 loaded, but no emulator constructor was exposed");
    }
  }

  async function startEmulator() {
    if (emulator) {
      return;
    }
    setStatus("Loading emulator", "ready");
    startButton.disabled = true;
    resetButton.disabled = true;
    try {
      await loadV86();
      placeholder.classList.add("is-hidden");
      const V86Constructor = window.V86 || window.V86Starter;
      emulator = new V86Constructor(emulatorConfig);
      window.taijiosEmulator = emulator;
      wireEmulatorEvents(emulator);
      setStatus("Booting", "running");
    } catch (error) {
      startButton.disabled = false;
      resetButton.disabled = true;
      setStatus("Could not start browser boot", "error");
      placeholder.classList.remove("is-hidden");
      console.error(error);
    }
  }

  function wireEmulatorEvents(instance) {
    instance.add_listener("download-progress", (event) => {
      if (event && event.file_name) {
        setStatus(`Loading ${shortName(event.file_name)}`, "ready");
      }
    });
    instance.add_listener("download-error", (event) => {
      const file = event && event.file_name ? `: ${shortName(event.file_name)}` : "";
      resetButton.disabled = true;
      setStatus(`Download failed${file}`, "error");
    });
    instance.add_listener("emulator-ready", () => {
      resetButton.disabled = false;
      setBootChoicesEnabled(true);
      maybeAutoBoot(instance);
      setStatus("Ready to boot", "ready");
    });
    instance.add_listener("emulator-started", () => {
      resetButton.disabled = false;
      setBootChoicesEnabled(true);
      setStatus("Running", "running");
    });
    instance.add_listener("emulator-stopped", () => {
      setStatus("Stopped", "ready");
    });
  }

  function shortName(path) {
    return path.split("/").pop() || path;
  }

  function setBootChoicesEnabled(enabled) {
    bootChoiceButtons.forEach((button) => {
      button.disabled = !enabled;
    });
  }

  function sendBootChoice(choice) {
    if (!emulator || typeof emulator.keyboard_send_text !== "function") {
      return;
    }
    emulator.keyboard_send_text(`${choice}\n`);
    setStatus(`Boot option ${choice} sent`, "running");
  }

  function maybeAutoBoot(instance) {
    if (autoBootQueued) {
      return;
    }
    const params = new URLSearchParams(window.location.search);
    if (params.get("boot") === "manual") {
      return;
    }
    const choice = params.get("boot") || "1";
    if (!/^[1-9]$/.test(choice)) {
      return;
    }
    autoBootQueued = true;
    instance.automatically([
      { vga_text: "Selection:" },
      { keyboard_send: `${choice}\n` },
    ]);
  }

  function resetEmulator() {
    if (emulator && typeof emulator.restart === "function" && !resetButton.disabled) {
      emulator.restart();
      setStatus("Restarting", "running");
    }
  }

  async function fullscreenEmulator() {
    if (!document.fullscreenElement) {
      await screenContainer.requestFullscreen();
    } else {
      await document.exitFullscreen();
    }
  }

  startButton.addEventListener("click", startEmulator);
  resetButton.addEventListener("click", resetEmulator);
  bootChoiceButtons.forEach((button) => {
    button.addEventListener("click", () => sendBootChoice(button.dataset.bootChoice));
  });
  fullscreenButton.addEventListener("click", () => {
    fullscreenEmulator().catch((error) => {
      setStatus("Fullscreen unavailable", "error");
      console.error(error);
    });
  });

  window.addEventListener("error", (event) => {
    if (emulator) {
      setStatus("Browser boot error", "error");
    }
    console.error(event.error || event.message);
  });

  window.addEventListener("unhandledrejection", (event) => {
    if (emulator) {
      setStatus("Browser boot error", "error");
    }
    console.error(event.reason);
  });

  setStatus("Ready", "ready");
  if (new URLSearchParams(window.location.search).get("autostart") === "1") {
    startEmulator();
  }
})();
