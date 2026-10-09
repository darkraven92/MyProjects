"""Source ownership guard for the Windows-only runtime entry points.

The portable C++ fixtures exercise evidence and suppression. These checks pin
the early-return boundary ahead of *all* existing gameplay lifetimes, including
destructors, and the bootstrap world wait. Live qualification remains separate.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ConnectionObserverBoundaryTest(unittest.TestCase):
    def test_monitor_dispatches_before_any_gameplay_lifetime(self):
        source = (ROOT / "src/Bot/WorldMonitor.h").read_text()
        run = source[source.index("static void Run()") :]
        expected = '''static void Run() {
            const auto connectionMode = ParseConnectionMode(
                std::getenv("WOW_INTERNAL_CONNECTION_MODE"));
            if (connectionMode != ConnectionMode::Normal) {
                ConnectionLifecycleObserver5875::Run(connectionMode);
                return;
            }
            struct NavMeshSessionLifetime'''
        self.assertTrue(re.sub(r'\s+', '', run).startswith(re.sub(r'\s+', '', expected)))
        # These existing owners must remain behind the unconditional return.
        boundary = run.index("struct NavMeshSessionLifetime")
        for owner in ("CombatController combat", "GrindModeController grindMode",
                      "SharedAfkController sharedAfk", "DeathRecoveryController deathRecovery",
                      "RuntimeRobustnessSupervisor runtimeRobustness",
                      "WaterEvidenceObserve5875::Run", "questPlannerRuntime.Initialize()",
                      "AutoAttackController::Stop()", "MovementController::HoldPosition"):
            self.assertGreater(run.index(owner), boundary, owner)

    def test_bootstrap_observer_bypasses_wait_and_probe_then_unloads(self):
        source = (ROOT / "src/dllmain.cpp").read_text()
        bootstrap = source[source.index("DWORD WINAPI BootstrapThread") :]
        guard = bootstrap.index('Bot::ParseConnectionMode(std::getenv("WOW_INTERNAL_CONNECTION_MODE"))')
        end = bootstrap.index("\n    }", guard)
        branch = bootstrap[guard:end]
        self.assertIn("Bot::ConnectionMode::Normal)", branch)
        self.assertLess(branch.index("Bot::WorldMonitor::Run();"), branch.index("UnloadSelf(module,"))
        self.assertIn("[[noreturn]] void UnloadSelf", source)
        self.assertLess(end, bootstrap.index("if (!WaitForWorld())"))
        self.assertLess(end, bootstrap.index("Objects::ObjectManagerProbe::Run()"))

    def test_observer_has_only_read_logging_and_ipc_dependencies(self):
        source = (ROOT / "src/Bot/ConnectionLifecycleObserver5875.h").read_text()
        self.assertEqual(re.findall(r'#include "([^"]+)"', source), [
            "ConnectionObservationPolicy.h", "../Control/RuntimeControl.h",
            "../Debug/Logger.h", "../Objects/WorldState.h", "../Wow5875/Client.h"])
        self.assertIn("ConnectionEvidence5875::Observe(", source)
        self.assertIn("ReadProcessMemory(", source)
        self.assertIn("Objects::WorldStateReader::Read(", source)
        self.assertIn("control.MarkRuntimeDetached();", source)
        for forbidden in ("Controller::", "GameThreadDispatcher", "ExecuteLua",
                          "WriteProcessMemory", "SendInput", "EnterWorld(",
                          "DefaultServerLogin(", "HoldPosition("):
            self.assertNotIn(forbidden, source)

    def test_observer_transitive_dependencies_have_no_action_adapter(self):
        pending = [ROOT / "src/Bot/ConnectionLifecycleObserver5875.h"]
        visited = set()
        while pending:
            path = pending.pop().resolve()
            if path in visited:
                continue
            visited.add(path)
            source = path.read_text()
            # Check the whole local include closure, not just Run's direct
            # includes. Logging/IPC writes are allowed; client/input writes,
            # dispatcher hooks, Lua execution and command adapters are not.
            for forbidden in ("GameThreadDispatcher", "ExecuteLua", "ExecuteScript",
                              "WriteProcessMemory", "SendInput", "keybd_event", "mouse_event",
                              "SetWindowsHookEx", "SetWindowLongPtr", "PostMessage", "SendMessage",
                              "DefaultServerLogin", "GlueDialog_OnClick", "EnterWorld("):
                self.assertNotIn(forbidden, source, str(path.relative_to(ROOT)))
            for include in re.findall(r'#include "([^"]+)"', source):
                pending.append(path.parent / include)
        self.assertFalse(any("Controller" in p.name for p in visited))
        self.assertTrue(any(p.name == "ConnectionEvidence5875.h" for p in visited))
