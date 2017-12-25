Description
===========

This example client shows how to receive overlays from a CloudTV server (if
client side overlays are enabled).

Overlays can be transmitted in-band over the RFB-TV connection or as URL
encoded image. The latter requires to have registered an implementation of the
IContentLoader interface to download the overlays from the given URL.

Nano SDK provides with a helper class DefaultContentLoader that can be used.
