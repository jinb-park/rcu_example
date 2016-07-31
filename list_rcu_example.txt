==================
list_rcu_example
==================

Table of Contents:

1. book management system
2. mapping between rcu operation and book operation



1. book management system
==============================

This example code is book management system by using rcu list.



2. mapping between book operation and rcu operation
===========================================================


There are 5 book operations. and Each operations is mapped with a rcu operation.
You can get information about implementation of each operation from source code.

	1) add_book	= RCU Updater
	2) borrow_book	= RCU Updater and Reclaimer
	3) return_book	= RCU Updater and Reclaimer
	4) is_borrowed_book	= RCU Reader
	5) delete_book	= RCU Updater and Reclaimer


