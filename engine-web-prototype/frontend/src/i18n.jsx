import {useSyncExternalStore} from 'react';
import {getLanguage,subscribeLanguage} from './i18n.js';
export * from './i18n.js';
export function useLocale() { return useSyncExternalStore(subscribeLanguage,getLanguage,()=> 'ru'); }
