// Keep the HTML splash visible while the application modules are loading.
import('./main.jsx').catch(error=>{
  console.error('Studio startup failed:',error);
  const splash=document.querySelector('[data-boot-splash]');
  if(!splash)return;
  splash.dataset.failed='true';
  splash.setAttribute('aria-busy','false');
  const status=splash.querySelector('[role="status"]');
  status.setAttribute('role','alert');
  status.textContent='Не удалось загрузить студию. Попробуйте ещё раз.';
  const retry=splash.querySelector('[data-boot-retry]');
  retry.hidden=false;
  retry.addEventListener('click',()=>location.reload());
});
