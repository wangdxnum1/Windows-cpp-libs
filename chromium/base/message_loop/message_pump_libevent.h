// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_

#include <memory>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"

// Declare structs we need from libevent.h rather than including it
struct event_base;
struct event;

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
class BASE_EXPORT MessagePumpLibevent : public MessagePump,
                                        public WatchableIOMessagePumpPosix {
 public:
  class FdWatchController : public FdWatchControllerInterface {
   public:
    explicit FdWatchController(const Location& from_here);

    FdWatchController(const FdWatchController&) = delete;
    FdWatchController& operator=(const FdWatchController&) = delete;

    // Implicitly calls StopWatchingFileDescriptor.
    ~FdWatchController() override;

    // FdWatchControllerInterface:
    bool StopWatchingFileDescriptor() override;

   private:
    friend class MessagePumpLibevent;
    friend class MessagePumpLibeventTest;

    // Called by MessagePumpLibevent.
    void Init(std::unique_ptr<event> e);

    // Used by MessagePumpLibevent to take ownership of |event_|.
    std::unique_ptr<event> ReleaseEvent();

    void set_pump(MessagePumpLibevent* pump) { pump_ = pump; }
    MessagePumpLibevent* pump() const { return pump_; }

    void set_watcher(FdWatcher* watcher) { watcher_ = watcher; }

    void OnFileCanReadWithoutBlocking(int fd, MessagePumpLibevent* pump);
    void OnFileCanWriteWithoutBlocking(int fd, MessagePumpLibevent* pump);

    std::unique_ptr<event> event_;
    raw_ptr<MessagePumpLibevent> pump_ = nullptr;
    raw_ptr<FdWatcher> watcher_ = nullptr;
    // If this pointer is non-NULL, the pointee is set to true in the
    // destructor.
    raw_ptr<bool> was_destroyed_ = nullptr;
  };

  MessagePumpLibevent();

  MessagePumpLibevent(const MessagePumpLibevent&) = delete;
  MessagePumpLibevent& operator=(const MessagePumpLibevent&) = delete;

  ~MessagePumpLibevent() override;

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* delegate);

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;

 private:
  friend class MessagePumpLibeventTest;

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // Called by libevent to tell us a registered FD can be read/written to.
  static void OnLibeventNotification(int fd, short flags, void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);

  struct RunState {
    explicit RunState(Delegate* delegate_in) : delegate(delegate_in) {}

    // `delegate` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION Delegate* const delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit = false;
  };

  // State for the current invocation of Run(). null if not running.
  RunState* run_state_ = nullptr;

  // This flag is set if libevent has processed I/O events.
  bool processed_io_events_ = false;

  // Libevent dispatcher.  Watches all sockets registered with it, and sends
  // readiness callbacks when a socket is ready for I/O.
  const raw_ptr<event_base, DanglingUntriaged> event_base_;

  // ... write end; ScheduleWork() writes a single byte to it
  int wakeup_pipe_in_ = -1;
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  int wakeup_pipe_out_ = -1;
  // ... libevent wrapper for read end
  raw_ptr<event, DanglingUntriaged> wakeup_event_ = nullptr;

  ThreadChecker watch_file_descriptor_caller_checker_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
