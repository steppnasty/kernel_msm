/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <typedefs.h>
#include <linux/kernel.h>
#include <linuxver.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wl_dbg.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wl_export.h>
#include <wlc_event.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_mac80211.h>
#ifdef MSGTRACE
#include <msgtrace.h>
#endif

/* Local prototypes */
static void wlc_timer_cb(void *arg);

/* Private data structures */
struct wlc_eventq {
	wlc_event_t *head;
	wlc_event_t *tail;
	struct wlc_info *wlc;
	void *wl;
	wlc_pub_t *pub;
	bool tpending;
	bool workpending;
	struct wl_timer *timer;
	wlc_eventq_cb_t cb;
	uint8 event_inds_mask[ROUNDUP(WLC_E_LAST, NBBY) / NBBY];
};

/*
 * Export functions
 */
wlc_eventq_t *BCMATTACHFN(wlc_eventq_attach) (wlc_pub_t *pub,
					      struct wlc_info *wlc, void *wl,
					      wlc_eventq_cb_t cb) {
	wlc_eventq_t *eq;

	eq = (wlc_eventq_t *) MALLOC(pub->osh, sizeof(wlc_eventq_t));
	if (eq == NULL)
		return NULL;

	bzero(eq, sizeof(wlc_eventq_t));

	eq->cb = cb;
	eq->wlc = wlc;
	eq->wl = wl;
	eq->pub = pub;

	eq->timer = wl_init_timer(eq->wl, wlc_timer_cb, eq, "eventq");
	if (!eq->timer) {
		WL_ERROR(("wl%d: wlc_eventq_attach: timer failed\n",
			  pub->unit));
		MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	return eq;
}

int BCMATTACHFN(wlc_eventq_detach) (wlc_eventq_t *eq)
{
	/* Clean up pending events */
	wlc_eventq_down(eq);

	if (eq->timer) {
		if (eq->tpending) {
			wl_del_timer(eq->wl, eq->timer);
			eq->tpending = FALSE;
		}
		wl_free_timer(eq->wl, eq->timer);
		eq->timer = NULL;
	}

	ASSERT(wlc_eventq_avail(eq) == FALSE);
	MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
	return 0;
}

int BCMUNINITFN(wlc_eventq_down) (wlc_eventq_t *eq)
{
	int callbacks = 0;
	if (eq->tpending && !eq->workpending) {
		if (!wl_del_timer(eq->wl, eq->timer))
			callbacks++;

		ASSERT(wlc_eventq_avail(eq) == TRUE);
		ASSERT(eq->workpending == FALSE);
		eq->workpending = TRUE;
		if (eq->cb)
			eq->cb(eq->wlc);

		ASSERT(eq->workpending == TRUE);
		eq->workpending = FALSE;
		eq->tpending = FALSE;
	} else {
		ASSERT(eq->workpending || wlc_eventq_avail(eq) == FALSE);
	}
	return callbacks;
}

wlc_event_t *wlc_event_alloc(wlc_eventq_t *eq)
{
	wlc_event_t *e;

	e = MALLOC(eq->pub->osh, sizeof(wlc_event_t));

	if (e == NULL)
		return NULL;

	bzero(e, sizeof(wlc_event_t));
	return e;
}

void wlc_event_free(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->data == NULL);
	ASSERT(e->next == NULL);
	MFREE(eq->pub->osh, e, sizeof(wlc_event_t));
}

void wlc_eventq_enq(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->next == NULL);
	e->next = NULL;

	if (eq->tail) {
		eq->tail->next = e;
		eq->tail = e;
	} else
		eq->head = eq->tail = e;

	if (!eq->tpending) {
		eq->tpending = TRUE;
		/* Use a zero-delay timer to trigger
		 * delayed processing of the event.
		 */
		wl_add_timer(eq->wl, eq->timer, 0, 0);
	}
}

wlc_event_t *wlc_eventq_deq(wlc_eventq_t *eq)
{
	wlc_event_t *e;

	e = eq->head;
	if (e) {
		eq->head = e->next;
		e->next = NULL;

		if (eq->head == NULL)
			eq->tail = eq->head;
	}
	return e;
}

wlc_event_t *wlc_eventq_next(wlc_eventq_t *eq, wlc_event_t *e)
{
#ifdef BCMDBG
	wlc_event_t *etmp;

	for (etmp = eq->head; etmp; etmp = etmp->next) {
		if (etmp == e)
			break;
	}
	ASSERT(etmp != NULL);
#endif

	return e->next;
}

int wlc_eventq_cnt(wlc_eventq_t *eq)
{
	wlc_event_t *etmp;
	int cnt = 0;

	for (etmp = eq->head; etmp; etmp = etmp->next)
		cnt++;

	return cnt;
}

bool wlc_eventq_avail(wlc_eventq_t *eq)
{
	return (eq->head != NULL);
}

/*
 * Local Functions
 */
static void wlc_timer_cb(void *arg)
{
	struct wlc_eventq *eq = (struct wlc_eventq *)arg;

	ASSERT(eq->tpending == TRUE);
	ASSERT(wlc_eventq_avail(eq) == TRUE);
	ASSERT(eq->workpending == FALSE);
	eq->workpending = TRUE;

	if (eq->cb)
		eq->cb(eq->wlc);

	ASSERT(wlc_eventq_avail(eq) == FALSE);
	ASSERT(eq->tpending == TRUE);
	eq->workpending = FALSE;
	eq->tpending = FALSE;
}
