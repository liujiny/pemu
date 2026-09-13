#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

/* The Payload SDK v0.42 headers expose gmtime_r, but its native import set
 * exports only gmtime. Serialize access to the libc-owned result and copy it
 * into caller storage to preserve the gmtime_r ownership contract. */
struct tm *gmtime_r(const time_t *timer, struct tm *result) {
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  struct tm *shared;

  if (timer == NULL || result == NULL) {
    errno = EINVAL;
    return NULL;
  }
  if (pthread_mutex_lock(&lock) != 0) {
    errno = EBUSY;
    return NULL;
  }
  shared = gmtime(timer);
  if (shared != NULL)
    memcpy(result, shared, sizeof(*result));
  pthread_mutex_unlock(&lock);
  return shared == NULL ? NULL : result;
}
