# P3. Message Queues

Using message queues design and implement a group messaging system. It should be able to allow users in a UNIX system to create groups, list groups, join, send private messages, send message to groups, receive message from groups in online/offline modes. A user may set auto delete `<t>` option which means, users who joined after *t* seconds from the time of message creation, to them message will not be delivered.

Deliverables:
- msgq_server.c, msgq_client.c
- pdf file explaining design decisions
