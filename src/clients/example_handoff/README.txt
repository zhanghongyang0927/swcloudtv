Description
===========

This example client shows how an RFB-TV session handoff handler could be implemented.

Session handoff is a feature that is introduced in RFB-TV 2.0

The handoff procedure starts when the server sends a HandoffRequest message to
the client. This message requests the client to terminate the session and perform
the handoff to an external program (or media player in case of e.g. a 'VoD' handoff).

Contrary to handoffs for other schemas, an "rfbtv(s)" handoff always has the
'resume_session_when_done' flag set to 'false'.
