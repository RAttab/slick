Slick
=====

Personal playground for network and distributed algorithms related doodads.
Current doodads worthy of notice:

- Endpoint: Basic TCP networking layer.

- Pack: Serialization framework.

- Discovery: P2P network which provides a loose key-value storage. Best use for
  endpoint discovery and was designed in direct oposition to zookeeper which is
  massive overkill for simple discovery tasks.

Disclaimer, if you use any part of this library in a production system, you
deserve all the miserable and horrible things that will happen to you.


How to Build
----------

First off, get these dependencies from your local software distribution center:

* cmake 2.6+
* gcc 4.7+
* gperftools' tcmalloc (optional but highly recomended).

Next, enter these two magical commands:

    cmake CMakeLists.txt
    make all test

That's it.



