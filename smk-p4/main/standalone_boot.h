#pragma once

namespace smk::p4::boot {
// Boot-only policy. The fresh mailbox has exactly one writer (the owner),
// which marks initialization complete before its first publication.
template<class Snapshot, class Peek, class Delay>
bool awaitSnapshot(bool mailbox_available, bool owner_started, Snapshot& snapshot,
                   unsigned polls, Peek peek, Delay delay) {
    snapshot = Snapshot{};
    if (!mailbox_available || !owner_started) return false;
    for (unsigned i = 0; i < polls; ++i) {
        if (peek(snapshot)) return true;
        delay();
    }
    return false;
}

enum class UsbStart { Ready, Disabled, MailboxUnavailable, OwnerNotStarted,
                      SnapshotTimeout, OwnerNotInitialized, OwnerFailed,
                      OwnerStopped, EngineNotReady };

template<class Snapshot>
UsbStart usbStart(bool enabled_and_qualified, bool mailbox_available,
                  bool owner_started, bool snapshot_received, const Snapshot& snapshot) {
    if (!enabled_and_qualified) return UsbStart::Disabled;
    if (!mailbox_available) return UsbStart::MailboxUnavailable;
    if (!owner_started) return UsbStart::OwnerNotStarted;
    if (!snapshot_received) return UsbStart::SnapshotTimeout;
    // Never inspect the payload without a successful mailbox query.
    if (!snapshot.owner_initialized) return UsbStart::OwnerNotInitialized;
    if (snapshot.error != 0) return UsbStart::OwnerFailed;
    if (!snapshot.running) return UsbStart::OwnerStopped;
    if (!snapshot.engine.ready) return UsbStart::EngineNotReady;
    return UsbStart::Ready;
}

inline const char* reason(UsbStart result) {
    switch (result) {
    case UsbStart::Ready: return "owner ready; wiring qualification enabled";
    case UsbStart::Disabled: return "USB disabled; wiring qualification not enabled";
    case UsbStart::MailboxUnavailable: return "snapshot mailbox unavailable";
    case UsbStart::OwnerNotStarted: return "audio owner task creation failed";
    case UsbStart::SnapshotTimeout: return "initial owner snapshot timeout";
    case UsbStart::OwnerNotInitialized: return "owner initialization not confirmed";
    case UsbStart::OwnerFailed: return "audio owner initialization/service failed";
    case UsbStart::OwnerStopped: return "audio owner not running";
    case UsbStart::EngineNotReady: return "AMY engine not ready";
    }
    return "unknown boot status";
}
}
