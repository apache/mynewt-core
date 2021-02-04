/*
   CC0 1.0 Universal

   Statement of Purpose

   The laws of most jurisdictions throughout the world automatically confer
   exclusive Copyright and Related Rights (defined below) upon the creator and
   subsequent owner(s) (each and all, an "owner") of an original work of
   authorship and/or a database (each, a "Work").

   Certain owners wish to permanently relinquish those rights to a Work for the
   purpose of contributing to a commons of creative, cultural and scientific
   works ("Commons") that the public can reliably and without fear of later
   claims of infringement build upon, modify, incorporate in other works, reuse
   and redistribute as freely as possible in any form whatsoever and for any
   purposes, including without limitation commercial purposes. These owners may
   contribute to the Commons to promote the ideal of a free culture and the
   further production of creative, cultural and scientific works, or to gain
   reputation or greater distribution for their Work in part through the use and
   efforts of others.

   For these and/or other purposes and motivations, and without any expectation
   of additional consideration or compensation, the person associating CC0 with a
   Work (the "Affirmer"), to the extent that he or she is an owner of Copyright
   and Related Rights in the Work, voluntarily elects to apply CC0 to the Work
   and publicly distribute the Work under its terms, with knowledge of his or her
   Copyright and Related Rights in the Work and the meaning and intended legal
   effect of CC0 on those rights.

   1. Copyright and Related Rights. A Work made available under CC0 may be
   protected by copyright and related or neighboring rights ("Copyright and
   Related Rights"). Copyright and Related Rights include, but are not limited
   to, the following:

   i. the right to reproduce, adapt, distribute, perform, display, communicate,
   and translate a Work;

   ii. moral rights retained by the original author(s) and/or performer(s);

   iii. publicity and privacy rights pertaining to a person's image or likeness
   depicted in a Work;

   iv. rights protecting against unfair competition in regards to a Work,
   subject to the limitations in paragraph 4(a), below;

   v. rights protecting the extraction, dissemination, use and reuse of data in
   a Work;

   vi. database rights (such as those arising under Directive 96/9/EC of the
   European Parliament and of the Council of 11 March 1996 on the legal
   protection of databases, and under any national implementation thereof,
   including any amended or successor version of such directive); and

   vii. other similar, equivalent or corresponding rights throughout the world
   based on applicable law or treaty, and any national implementations thereof.

   2. Waiver. To the greatest extent permitted by, but not in contravention of,
   applicable law, Affirmer hereby overtly, fully, permanently, irrevocably and
   unconditionally waives, abandons, and surrenders all of Affirmer's Copyright
   and Related Rights and associated claims and causes of action, whether now
   known or unknown (including existing as well as future claims and causes of
   action), in the Work (i) in all territories worldwide, (ii) for the maximum
   duration provided by applicable law or treaty (including future time
   extensions), (iii) in any current or future medium and for any number of
   copies, and (iv) for any purpose whatsoever, including without limitation
   commercial, advertising or promotional purposes (the "Waiver"). Affirmer makes
   the Waiver for the benefit of each member of the public at large and to the
   detriment of Affirmer's heirs and successors, fully intending that such Waiver
   shall not be subject to revocation, rescission, cancellation, termination, or
   any other legal or equitable action to disrupt the quiet enjoyment of the Work
   by the public as contemplated by Affirmer's express Statement of Purpose.

   3. Public License Fallback. Should any part of the Waiver for any reason be
   judged legally invalid or ineffective under applicable law, then the Waiver
   shall be preserved to the maximum extent permitted taking into account
   Affirmer's express Statement of Purpose. In addition, to the extent the Waiver
   is so judged Affirmer hereby grants to each affected person a royalty-free,
   non transferable, non sublicensable, non exclusive, irrevocable and
   unconditional license to exercise Affirmer's Copyright and Related Rights in
   the Work (i) in all territories worldwide, (ii) for the maximum duration
   provided by applicable law or treaty (including future time extensions), (iii)
   in any current or future medium and for any number of copies, and (iv) for any
   purpose whatsoever, including without limitation commercial, advertising or
   promotional purposes (the "License"). The License shall be deemed effective as
   of the date CC0 was applied by Affirmer to the Work. Should any part of the
   License for any reason be judged legally invalid or ineffective under
   applicable law, such partial invalidity or ineffectiveness shall not
   invalidate the remainder of the License, and in such case Affirmer hereby
   affirms that he or she will not (i) exercise any of his or her remaining
   Copyright and Related Rights in the Work or (ii) assert any associated claims
   and causes of action with respect to the Work, in either case contrary to
   Affirmer's express Statement of Purpose.

   4. Limitations and Disclaimers.

   a. No trademark or patent rights held by Affirmer are waived, abandoned,
   surrendered, licensed or otherwise affected by this document.

   b. Affirmer offers the Work as-is and makes no representations or warranties
   of any kind concerning the Work, express, implied, statutory or otherwise,
   including without limitation warranties of title, merchantability, fitness
   for a particular purpose, non infringement, or the absence of latent or
   other defects, accuracy, or the present or absence of errors, whether or not
   discoverable, all to the greatest extent permissible under applicable law.

   c. Affirmer disclaims responsibility for clearing rights of other persons
   that may apply to the Work or any use thereof, including without limitation
   any person's Copyright and Related Rights in the Work. Further, Affirmer
   disclaims responsibility for obtaining any necessary consents, permissions
   or other rights required for any use of the Work.

   d. Affirmer understands and acknowledges that Creative Commons is not a
   party to this document and has no duty or obligation with respect to this
   CC0 or use of the Work.

   For more information, please see
   <http://creativecommons.org/publicdomain/zero/1.0/>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "osdp/circular_buffer.h"

/* The definition of our circular buffer structure is hidden from the user */
struct circular_buf_t {
    uint8_t *buffer;
    size_t head;
    size_t tail;
    size_t max; /*of the buffer */
    bool full;
};

static void
advance_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf);

    if (cbuf->full) {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }

    cbuf->head = (cbuf->head + 1) % cbuf->max;

    /* We mark full because we will advance tail on the next time around */
    cbuf->full = (cbuf->head == cbuf->tail);
}

static void
retreat_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf);

    cbuf->full = false;
    cbuf->tail = (cbuf->tail + 1) % cbuf->max;
}

cbuf_handle_t
circular_buf_init(uint8_t *buffer, size_t size)
{
    assert(buffer && size);

    cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));
    assert(cbuf);

    cbuf->buffer = buffer;
    cbuf->max = size;
    circular_buf_reset(cbuf);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

void
circular_buf_free(cbuf_handle_t cbuf)
{
    assert(cbuf);
    free(cbuf);
}

void
circular_buf_reset(cbuf_handle_t cbuf)
{
    assert(cbuf);

    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->full = false;
}

size_t
circular_buf_size(cbuf_handle_t cbuf)
{
    assert(cbuf);

    size_t size = cbuf->max;

    if (!cbuf->full) {
        if (cbuf->head >= cbuf->tail) {
            size = (cbuf->head - cbuf->tail);
        } else {
            size = (cbuf->max + cbuf->head - cbuf->tail);
        }

    }

    return size;
}

size_t
circular_buf_capacity(cbuf_handle_t cbuf)
{
    assert(cbuf);

    return cbuf->max;
}

void
circular_buf_put(cbuf_handle_t cbuf, uint8_t data)
{
    assert(cbuf && cbuf->buffer);

    cbuf->buffer[cbuf->head] = data;

    advance_pointer(cbuf);
}

int
circular_buf_put2(cbuf_handle_t cbuf, uint8_t data)
{
    int r = -1;

    assert(cbuf && cbuf->buffer);

    if (!circular_buf_full(cbuf)) {
        cbuf->buffer[cbuf->head] = data;
        advance_pointer(cbuf);
        r = 0;
    }

    return r;
}

int
circular_buf_get(cbuf_handle_t cbuf, uint8_t *data)
{
    assert(cbuf && data && cbuf->buffer);

    int r = -1;

    if (!circular_buf_empty(cbuf)) {
        *data = cbuf->buffer[cbuf->tail];
        retreat_pointer(cbuf);

        r = 0;
    }

    return r;
}

bool
circular_buf_empty(cbuf_handle_t cbuf)
{
    assert(cbuf);

    return (!cbuf->full && (cbuf->head == cbuf->tail));
}

bool
circular_buf_full(cbuf_handle_t cbuf)
{
    assert(cbuf);

    return cbuf->full;
}
