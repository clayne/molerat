#include "tasklet.h"

struct test_tasklet {
	struct mutex mutex;
	struct tasklet tasklet;
	struct wait_list *sema;
	unsigned int got;
};

void test_tasklet_wait(void *v_tt)
{
	struct test_tasklet *tt = v_tt;

	while (wait_list_down(tt->sema, 1, &tt->tasklet))
		tt->got++;

	mutex_unlock(&tt->mutex);
}

struct test_tasklet *test_tasklet_create(struct wait_list *sema)
{
	struct test_tasklet *tt = xalloc(sizeof *tt);

	mutex_init(&tt->mutex);
	tasklet_init(&tt->tasklet, &tt->mutex, tt);
	tt->sema = sema;
	tt->got = 0;

	mutex_lock(&tt->mutex);
	tasklet_now(&tt->tasklet, test_tasklet_wait);

	return tt;
}

void test_tasklet_destroy(struct test_tasklet *tt)
{
	mutex_lock(&tt->mutex);
	tasklet_fini(&tt->tasklet);
	mutex_unlock_fini(&tt->mutex);
	free(tt);
}

static void test_wait_list(void)
{
	struct run_queue *runq = run_queue_create();
	int count = 3;
	int i, total_got;
	struct test_tasklet **tts = xalloc(count * sizeof *tts);
	struct wait_list sema;

	run_queue_target(runq);

	wait_list_init(&sema, 0);

	for (i = 0; i < count; i++)
		tts[i] = test_tasklet_create(&sema);

	wait_list_broadcast(&sema);
	run_queue_run(runq, FALSE);

	for (i = 0; i < count; i++) {
		wait_list_up(&sema, 2);
		run_queue_run(runq, FALSE);
	}

	total_got = 0;
	for (i = 0; i < count; i++)
		total_got += tts[i]->got;

	assert(total_got == count * 2);

	for (i = 0; i < count; i++)
		test_tasklet_destroy(tts[i]);

	wait_list_fini(&sema);
	free(tts);
	run_queue_target(NULL);
}

/* Wait a millisecond */
static void delay(void)
{
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;
        assert(!nanosleep(&ts, NULL));
}

struct trqw {
	struct run_queue *runq;
	bool_t ran;

	struct mutex mutex;
	struct tasklet tasklet;
};

static void test_run_queue_waiting_done(void *v_t)
{
	struct trqw *t = v_t;

	t->ran = TRUE;
	tasklet_fini(&t->tasklet);
	mutex_unlock_fini(&t->mutex);
}

static void test_run_queue_waiting_thread(void *v_t)
{
	struct trqw *t = v_t;

	mutex_init(&t->mutex);
	tasklet_init(&t->tasklet, &t->mutex, t);

	run_queue_target(t->runq);
	delay();
	tasklet_later(&t->tasklet, test_run_queue_waiting_done);
}

static void test_run_queue_waiting(void)
{
	struct thread thr;
	struct trqw t;

	t.runq = run_queue_create();
	t.ran = FALSE;

	thread_init(&thr, test_run_queue_waiting_thread, &t);
	run_queue_run(t.runq, TRUE);
	assert(t.ran);
	thread_fini(&thr);
}

int main(void)
{
	test_wait_list();
	test_run_queue_waiting();
        return 0;
}
