/*
 * list_rcu_example.c - list rcu sample module
 *
 * Copyright (C) 2016 Jinbum Park <jinb.park7@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/preempt.h>

/**
 * struct book - a book
 *
 * @borrow:	If it is 0, book is not borrowed. it is 1, book is borrowed.
 */
struct book {
	int id;
	char name[64];
	char author[64];
	int borrow;
	struct list_head node;
	struct rcu_head rcu;
};

static LIST_HEAD(books);
static spinlock_t books_lock;

/**
 * callback function for async-reclaim
 *
 * call_rcu() 		:  callback function is called when finish to wait every grace periods (async)
 * synchronize_rcu() :  wait to finish every grace periods (sync)
*/
static void book_reclaim_callback(struct rcu_head *rcu) {
	struct book *b = container_of(rcu, struct book, rcu);

	/**
	 * Why print preemt_count??
	 *
	 * To check whether this callback is atomic context or not.
	 * preempt_count here is more than 0. Because it is irq context.
	*/
	pr_info("callback free : %lx, preempt_count : %d\n", (unsigned long)b, preempt_count());
	kfree(b);
}

static void add_book(int id, const char *name, const char *author) {
	struct book *b;

	b = kzalloc(sizeof(struct book), GFP_KERNEL);
	if(!b)
		return;

	b->id = id;
	strncpy(b->name, name, sizeof(b->name));
	strncpy(b->author, author, sizeof(b->author));
	b->borrow = 0;

	/**
	 * list_add_rcu
	 *
	 * add_node(writer - add) use spin_lock()
	*/
	spin_lock(&books_lock);
	list_add_rcu(&b->node, &books);
	spin_unlock(&books_lock);
}

static int borrow_book(int id, int async) {
	struct book *b = NULL;
	struct book *new_b = NULL;
	struct book *old_b = NULL;

	/**
	 * updater
	 *
	 * (updater) require that alloc new node & copy, update new node & reclaim old node
	 * list_replace_rcu() is used to do that.
	*/
	rcu_read_lock();

	list_for_each_entry(b, &books, node) {
		if(b->id == id) {
			if(b->borrow) {
				rcu_read_unlock();
				return -1;
			}

			old_b = b;
			break;
		}
	}

	if(!old_b) {
		rcu_read_unlock();
		return -1;
	}

	new_b = kzalloc(sizeof(struct book), GFP_ATOMIC);
	if(!new_b) {
		rcu_read_unlock();
		return -1;
	}

	memcpy(new_b, old_b, sizeof(struct book));
	new_b->borrow = 1;
	
	spin_lock(&books_lock);
	list_replace_rcu(&old_b->node, &new_b->node);
	spin_unlock(&books_lock);

	rcu_read_unlock();

	if(async) {
		call_rcu(&old_b->rcu, book_reclaim_callback);
	}else {
		synchronize_rcu();
		kfree(old_b);
	}

	pr_info("borrow success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static int is_borrowed_book(int id) {
	struct book *b;

	/**
	 * reader
	 *
	 * iteration(read) require rcu_read_lock(), rcu_read_unlock()
	 * and use list_for_each_entry_rcu()
	*/
	rcu_read_lock();
	list_for_each_entry_rcu(b, &books, node) {
		if(b->id == id) {
			rcu_read_unlock();
			return b->borrow;
		}
	}
	rcu_read_unlock();

	pr_err("not exist book\n");
	return -1;
}

static int return_book(int id, int async) {
	struct book *b = NULL;
	struct book *new_b = NULL;
	struct book *old_b = NULL;

	/**
	 * updater
	 *
	 * (updater) require that alloc new node & copy, update new node & reclaim old node
	 * list_replace_rcu() is used to do that.
	*/
	rcu_read_lock();

	list_for_each_entry(b, &books, node) {
		if(b->id == id) {
			if(!b->borrow) {
				rcu_read_unlock();
				return -1;
			}

			old_b = b;
			break;
		}
	}

	if(!old_b) {
		rcu_read_unlock();
		return -1;
	}

	new_b = kzalloc(sizeof(struct book), GFP_ATOMIC);
	if(!new_b) {
		rcu_read_unlock();
		return -1;
	}

	memcpy(new_b, old_b, sizeof(struct book));
	new_b->borrow = 0;
	
	spin_lock(&books_lock);
	list_replace_rcu(&old_b->node, &new_b->node);
	spin_unlock(&books_lock);

	rcu_read_unlock();

	if(async) {
		call_rcu(&old_b->rcu, book_reclaim_callback);
	}else {
		synchronize_rcu();
		kfree(old_b);
	}

	pr_info("return success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static void delete_book(int id, int async) {
	struct book *b;

	spin_lock(&books_lock);
	list_for_each_entry(b, &books, node) {
		if(b->id == id) {
			/**
			 * list_del
			 *
			 * del_node(writer - delete) require locking mechanism.
			 * we can choose 3 ways to lock. Use 'a' here.
			 *
			 *	a.	locking,
			 *	b.	atomic operations, or
			 *	c.	restricting updates to a single task.
			*/
			list_del_rcu(&b->node);
			spin_unlock(&books_lock);

			if(async) {
				call_rcu(&b->rcu, book_reclaim_callback);
			}else {
				synchronize_rcu();
				kfree(b);
			}
			return;
		}
	}
	spin_unlock(&books_lock);

	pr_err("not exist book\n");
}

static void print_book(int id) {
	struct book *b;

	rcu_read_lock();
	list_for_each_entry_rcu(b, &books, node) {
		if(b->id == id) {
			/**
			 * Why print address of "struct book *b"??
			 *
			 * If b was updated, address of b must be different.
			 * We can know whether b is updated or not by address.
			*/
			pr_info("id : %d, name : %s, author : %s, borrow : %d, addr : %lx\n", \
						b->id, b->name, b->author, b->borrow, (unsigned long)b);
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();

	pr_err("not exist book\n");
}

static void test_example(int async) {
	add_book(0, "book1", "jb");
	add_book(1, "book2", "jb");

	print_book(0);
	print_book(1);

	pr_info("book1 borrow : %d\n", is_borrowed_book(0));
	pr_info("book2 borrow : %d\n", is_borrowed_book(1));

	borrow_book(0, async);
	borrow_book(1, async);

	print_book(0);
	print_book(1);

	return_book(0, async);
	return_book(1, async);

	print_book(0);
	print_book(1);

	delete_book(0, async);
	delete_book(1, async);

	print_book(0);
	print_book(1);
}

static int list_rcu_example_init(void)
{
	spin_lock_init(&books_lock);

	test_example(0);
	test_example(1);
	return 0;
}

static void list_rcu_example_exit(void)
{
	return;
}

module_init(list_rcu_example_init);
module_exit(list_rcu_example_exit);
MODULE_LICENSE("GPL");
