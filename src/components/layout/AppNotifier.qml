pragma Singleton
import QtQuick

import CentralLogger.Core

// Central singleton for application-level toast and detail-dialog notifications.
//
// AppNotifier.show(summary, semantic, options?)
//   options: { detailText, detailTitle, copyPath, loggerId, durationMs }
//   copyPath set → toast tap copies that path to the clipboard
QtObject {
    id: root

    property string toastSummary:  ""
    property string toastSemantic: "info"
    property bool   toastVisible:  false
    property int    toastDurationMs: 5000

    property string pendingCopyPath: ""

    property string pendingDetailText:     ""
    property string pendingDetailTitle:    ""
    property int    pendingDetailLoggerId: -1

    property bool suppressed: false

    function show(summary, semantic, options) {
        if (suppressed) return
        toastSummary   = summary  || ""
        toastSemantic  = semantic || "info"
        toastDurationMs = (options && options.durationMs > 0) ? options.durationMs : 5000
        pendingCopyPath = (options && options.copyPath) ? options.copyPath : ""
        pendingDetailText    = (options && options.detailText)  ? options.detailText  : ""
        pendingDetailTitle   = (options && options.detailTitle) ? options.detailTitle : (summary || "")
        pendingDetailLoggerId = (options && options.loggerId !== undefined && options.loggerId >= 0)
                                ? options.loggerId : -1
        toastVisible = true
    }

    function dismiss() {
        toastVisible = false
    }

    function openDetail(title, body, loggerId) {
        root.detailRequested(title || "", body || "", (loggerId !== undefined && loggerId >= 0) ? loggerId : -1)
    }

    signal detailRequested(string title, string body, int loggerId)
}
