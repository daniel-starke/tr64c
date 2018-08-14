/**
 * @file bsearch.c
 * @author Daniel Starke
 * @see bsearch.h
 * @date 2016-01-16
 * @version 2018-07-08
 * 
 * DISCLAIMER
 * This file has no copyright assigned and is placed in the Public Domain.
 * All contributions are also assumed to be in the Public Domain.
 * Other contributions are not permitted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stddef.h>
#include "bsearch.h"


/**
 * Finds an element in a sorted array in O(log n). This is a drop-in replacement for bsearch with
 * more sanity checks.
 * 
 * @param[in] element - search for this element
 * @param[in] array - search within this array
 * @param[in] elementSize - size of a single array element
 * @param[in] size - number of elements in the array
 * @param[in] cmp - element compare function
 * @return pointer to the found element or NULL
 * @warning May return NULL if the passed array is not sorted correctly.
 */
const void * bs_array(const void * element, const void * array, const size_t elementSize, const size_t size, const BinarySearchComparerO cmp) {
	if (element == NULL || array == NULL || elementSize == 0 || size == 0 || cmp == NULL) return NULL;
	size_t first, current, last;
	first = 0;
	last = (size_t)(size - 1);
	current = last >> 1;
	#define BS_ARRAY_ITEM(x) ((const void *)(((const char *)array) + ((x) * elementSize)))
	while (first <= last && current < size) {
		const int order = cmp(element, BS_ARRAY_ITEM(current));
		if (order > 0) {
			first = current + 1;
		} else if (order == 0) {
			/* found item */
			return BS_ARRAY_ITEM(current);
		} else {
			last = (size_t)(current - 1);
		}
		current = (first + last) >> 1;
	}
	#undef BS_ARRAY_ITEM
	/* item not found */
	return NULL;
}
