#pragma once

#include "../Debug/Logger.h"

#include <windows.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace Bot
{
    class GameThreadDispatcher
    {
    private:
        static inline HWND window_ =
            nullptr;

        static inline DWORD windowThreadId_ =
            0;

        static inline UINT dispatchMessage_ =
            0;

        static inline HHOOK hook_ =
            nullptr;

        static inline std::mutex mutex_;

        static inline std::function<void()>
            pendingAction_;

        static inline bool actionExecuted_ =
            false;

        static BOOL CALLBACK EnumWindowsCallback(
            HWND hwnd,
            LPARAM)
        {
            DWORD processId =
                0;

            GetWindowThreadProcessId(
                hwnd,
                &processId
            );

            if (
                processId !=
                GetCurrentProcessId())
            {
                return TRUE;
            }

            if (!IsWindowVisible(hwnd))
            {
                return TRUE;
            }

            char title[512]{};

            const int length =
                GetWindowTextA(
                    hwnd,
                    title,
                    static_cast<int>(
                        sizeof(title)
                    )
                );

            if (length <= 0)
            {
                return TRUE;
            }

            const std::string windowTitle(
                title,
                static_cast<std::size_t>(
                    length
                )
            );

            if (
                windowTitle.find(
                    "World of Warcraft"
                ) ==
                std::string::npos)
            {
                return TRUE;
            }

            window_ =
                hwnd;

            return FALSE;
        }

        static bool ResolveWindow()
        {
            if (
                window_ != nullptr &&
                IsWindow(window_))
            {
                if (windowThreadId_ != 0)
                {
                    return true;
                }
            }

            window_ =
                nullptr;

            windowThreadId_ =
                0;

            Debug::Logger::Info(
                "GameThreadDispatcher: "
                "searching for WoW window..."
            );

            EnumWindows(
                EnumWindowsCallback,
                0
            );

            if (window_ == nullptr)
            {
                Debug::Logger::Info(
                    "GameThreadDispatcher FAILED: "
                    "WoW window not found."
                );

                return false;
            }

            windowThreadId_ =
                GetWindowThreadProcessId(
                    window_,
                    nullptr
                );

            if (windowThreadId_ == 0)
            {
                Debug::Logger::Info(
                    "GameThreadDispatcher FAILED: "
                    "window thread not found."
                );

                window_ =
                    nullptr;

                return false;
            }

            char title[512]{};

            GetWindowTextA(
                window_,
                title,
                static_cast<int>(
                    sizeof(title)
                )
            );

            Debug::Logger::Info(
                std::string(
                    "GameThreadDispatcher window: "
                ) +
                title
            );

            Debug::Logger::Info(
                "GameThreadDispatcher window thread: " +
                std::to_string(
                    windowThreadId_
                )
            );

            Debug::Logger::Info(
                "GameThreadDispatcher current thread: " +
                std::to_string(
                    GetCurrentThreadId()
                )
            );

            return true;
        }

        static bool ResolveDispatchMessage()
        {
            if (dispatchMessage_ != 0)
            {
                return true;
            }

            /*
             * RegisterWindowMessage gives us a
             * process/system-wide unique message ID.
             *
             * WoW does not need to know anything
             * about this message.
             */
            dispatchMessage_ =
                RegisterWindowMessageA(
                    "wow-internal-5875-"
                    "game-thread-dispatch"
                );

            if (dispatchMessage_ == 0)
            {
                Debug::Logger::Info(
                    "GameThreadDispatcher FAILED: "
                    "RegisterWindowMessage failed."
                );

                return false;
            }

            Debug::Logger::Info(
                "GameThreadDispatcher message registered: " +
                std::to_string(
                    dispatchMessage_
                )
            );

            return true;
        }

        static LRESULT CALLBACK HookProc(
            int code,
            WPARAM wParam,
            LPARAM lParam)
        {
            if (
                code >= 0 &&
                lParam != 0)
            {
                const auto* message =
                    reinterpret_cast<
                        const CWPSTRUCT*
                    >(lParam);

                if (
                    message->hwnd ==
                        window_ &&
                    message->message ==
                        dispatchMessage_)
                {
                    std::function<void()>
                        action;

                    {
                        std::lock_guard<std::mutex>
                            lock(mutex_);

                        action =
                            std::move(
                                pendingAction_
                            );

                        pendingAction_ =
                            nullptr;

                        actionExecuted_ =
                            false;
                    }

                    bool executed =
                        false;

                    if (action)
                    {
                        try
                        {
                            Debug::Logger::Info(
                                "GameThreadDispatcher: "
                                "hook received dispatch message."
                            );

                            Debug::Logger::Info(
                                "GameThreadDispatcher hook thread: " +
                                std::to_string(
                                    GetCurrentThreadId()
                                )
                            );

                            action();

                            executed =
                                true;
                        }
                        catch (...)
                        {
                            Debug::Logger::Info(
                                "GameThreadDispatcher: "
                                "exception while executing "
                                "hook action."
                            );
                        }
                    }

                    {
                        std::lock_guard<std::mutex>
                            lock(mutex_);

                        actionExecuted_ =
                            executed;
                    }
                }
            }

            /*
             * Always continue the hook chain.
             */
            return CallNextHookEx(
                hook_,
                code,
                wParam,
                lParam
            );
        }

    public:
        static bool Initialize()
        {
            if (!ResolveWindow())
            {
                return false;
            }

            if (!ResolveDispatchMessage())
            {
                return false;
            }

            Debug::Logger::Info(
                "GameThreadDispatcher ready "
                "(WH_CALLWNDPROC, no WndProc replacement)."
            );

            return true;
        }

        /*
         * Historical name kept because
         * ClickToMoveController already uses it.
         *
         * More precisely this tests whether we
         * are executing on WoW's window-owner
         * thread.
         */
        static bool IsGameThread()
        {
            if (windowThreadId_ == 0)
            {
                return false;
            }

            return
                GetCurrentThreadId() ==
                windowThreadId_;
        }

        static DWORD WindowThreadId()
        {
            return windowThreadId_;
        }

        static bool Invoke(
            std::function<void()> action)
        {
            if (!action)
            {
                return false;
            }

            if (!ResolveWindow())
            {
                return false;
            }

            if (!ResolveDispatchMessage())
            {
                return false;
            }

            /*
             * Already executing on the target
             * window thread.
             */
            if (IsGameThread())
            {
                Debug::Logger::Info(
                    "GameThreadDispatcher: "
                    "already on window thread."
                );

                action();

                return true;
            }

            {
                std::lock_guard<std::mutex>
                    lock(mutex_);

                if (pendingAction_)
                {
                    Debug::Logger::Info(
                        "GameThreadDispatcher rejected: "
                        "another action is pending."
                    );

                    return false;
                }

                pendingAction_ =
                    std::move(action);

                actionExecuted_ =
                    false;
            }

            /*
             * Important:
             *
             * This is a THREAD-SPECIFIC hook.
             *
             * We do not replace WoW's WndProc.
             *
             * Because the target thread belongs
             * to our current process, hMod is NULL.
             */
            hook_ =
                SetWindowsHookExA(
                    WH_CALLWNDPROC,
                    HookProc,
                    nullptr,
                    windowThreadId_
                );

            if (hook_ == nullptr)
            {
                const DWORD error =
                    GetLastError();

                Debug::Logger::Info(
                    "GameThreadDispatcher FAILED: "
                    "SetWindowsHookEx error=" +
                    std::to_string(
                        error
                    )
                );

                std::lock_guard<std::mutex>
                    lock(mutex_);

                pendingAction_ =
                    nullptr;

                return false;
            }

            Debug::Logger::Info(
                "GameThreadDispatcher: "
                "WH_CALLWNDPROC hook installed."
            );

            /*
             * SendMessage is synchronous.
             *
             * Before WoW's WndProc receives this
             * message, Windows calls HookProc on
             * the destination/window thread.
             */
            SendMessageA(
                window_,
                dispatchMessage_,
                0,
                0
            );

            const HHOOK hookToRemove =
                hook_;

            hook_ =
                nullptr;

            if (hookToRemove != nullptr)
            {
                if (!UnhookWindowsHookEx(
                        hookToRemove))
                {
                    Debug::Logger::Info(
                        "GameThreadDispatcher WARNING: "
                        "UnhookWindowsHookEx failed. "
                        "error=" +
                        std::to_string(
                            GetLastError()
                        )
                    );
                }
                else
                {
                    Debug::Logger::Info(
                        "GameThreadDispatcher: "
                        "WH_CALLWNDPROC hook removed."
                    );
                }
            }

            bool executed =
                false;

            {
                std::lock_guard<std::mutex>
                    lock(mutex_);

                executed =
                    actionExecuted_;

                pendingAction_ =
                    nullptr;
            }

            if (!executed)
            {
                Debug::Logger::Info(
                    "GameThreadDispatcher FAILED: "
                    "hook action was not executed."
                );

                return false;
            }

            Debug::Logger::Info(
                "GameThreadDispatcher: "
                "hook dispatch completed."
            );

            return true;
        }
    };
}
