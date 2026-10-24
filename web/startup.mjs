// A failed module/worker load may follow a deployment while HTML is cached.
// Keep reload explicit so the user can retain any completed local results.
export function revealReload(document,href,now=Date.now()){
  let link=document.getElementById('reload-app');
  if(!link){link=document.createElement('a');link.id='reload-app';link.textContent='Reload workbench ↻';(document.querySelector('header')??document.body).append(link);}
  const target=new URL('./',href);target.searchParams.set('reload',now.toString(36));
  link.href=target.href;link.hidden=false;
  document.documentElement?.classList.add('load-failed');
  return link;
}
