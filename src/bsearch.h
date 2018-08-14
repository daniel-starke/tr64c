/**
 * @file bsearch.h
 * @author Daniel Starke
 * @see bsearch.c
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
#ifndef __LIBPCF_BSEARCH_H__
#define __LIBPCF_BSEARCH_H__

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines the callback function to compare two elements. It is recommended to make the callback
 * function inline for speed increase. The first argument is the key to be compared and the second
 * the currently tested item from the given array.
 * 
 * @param[in] lhs - left hand statement
 * @param[in] rhs - right hand statement
 * @return <0 if lhs < rhs, 0 if lhs == rhs, >0 if lhs > rhs
 */
typedef int (* BinarySearchComparerO)(const void *, const void *);


const void * bs_array(const void * element, const void * array, const size_t elementSize, const size_t size, const BinarySearchComparerO cmp);


/**
 * Finds an element in a sorted static array in O(log n).
 * 
 * @param[in] element - search for this element
 * @param[in] array - search within this array
 * @param[in] cmp - element compare function
 * @return pointer to the found element or NULL
 * @see bs_array()
 * @warning May return NULL if the passed array is not sorted correctly.
 */
#define bs_staticArray(element, array, cmp) bs_array((element), (array), sizeof(*(array)), (size_t)(sizeof(array) / sizeof(*array)), (BinarySearchComparerO)(cmp))


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_CBUFFER_H__ */
