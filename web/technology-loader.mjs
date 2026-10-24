import {parseTechnology} from './technology.mjs';

// A file read or a process download can finish after a newer selection.
// Validate before publishing, and let only the latest selection change the UI.
export function createTechnologyLoader({apply,loading,error}) {
  let revision=0;
  return async function load(read,selection) {
    const current=++revision;
    loading(true);
    try {
      const text=await read();
      if(current!==revision)return false;
      const technology=parseTechnology(text);
      loading(false);
      apply({text,technology,selection});
      return true;
    } catch(reason) {
      if(current!==revision)return false;
      loading(false);
      error(reason);
      return false;
    }
  };
}
