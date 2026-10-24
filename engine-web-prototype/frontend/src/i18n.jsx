import {useSyncExternalStore} from 'react';
import {getLanguage,subscribeLanguage,GetLocaleRevision} from './i18n.js';
export * from './i18n.js';
export function useLocale() {
  useSyncExternalStore(subscribeLanguage,GetLocaleRevision,() => 0);
  return getLanguage();
}
