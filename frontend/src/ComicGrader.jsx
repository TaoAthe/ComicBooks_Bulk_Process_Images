import React, { useState, useEffect, useMemo } from 'react';
import {
  FolderOpen,
  FileText,
  Trash2,
  Play,
  RefreshCw,
  ServerCog,
  List,
  Image as ImageIcon,
  Loader2,
  AlertTriangle,
  CheckCircle2,
  ChevronUp,
  ChevronDown
} from 'lucide-react';

const API_BASE = 'http://localhost:5500';

const serverOptions = [
  { label: 'LM Studio', value: 'LMStudio' },
  { label: 'Ollama', value: 'Ollama' }
];

async function fetchJson(path, options = {}) {
  const url = typeof path === 'string' ? new URL(path, API_BASE) : path;
  const { headers, body, ...rest } = options;
  const response = await fetch(url, {
    ...rest,
    body,
    headers: {
      ...(body ? { 'Content-Type': 'application/json' } : {}),
      ...headers
    }
  });
  const text = await response.text();
  if (!response.ok) {
    const message = text || `Request failed (${response.status})`;
    throw new Error(message);
  }
  if (!text) {
    return null;
  }
  return JSON.parse(text);
}

function formatCurrency(value) {
  if (!value) return '—';
  const parsed = Number(String(value).replace(/[^0-9.]/g, ''));
  if (Number.isNaN(parsed)) return value;
  return `$${parsed.toLocaleString()}`;
}

export default function ComicGrader() {
  const [context, setContext] = useState({ rootPath: '', currentPath: '' });
  const [directory, setDirectory] = useState({ directories: [], files: [], parent: null });
  const [queue, setQueue] = useState([]);
  const [status, setStatus] = useState({ processing: false });
  const [results, setResults] = useState([]);
  const [server, setServer] = useState('LMStudio');
  const [models, setModels] = useState([]);
  const [selectedModel, setSelectedModel] = useState('');
  const [loadingModels, setLoadingModels] = useState(false);
  const [message, setMessage] = useState(null);
  const [error, setError] = useState(null);
  const [expandedResultId, setExpandedResultId] = useState(null);
  const [generateEbay, setGenerateEbay] = useState(false);

  const refreshContext = async () => {
    const data = await fetchJson('/api/context');
    setContext(data);
  };

  const loadDirectory = async (path, stickToCurrent = false) => {
    const params = new URLSearchParams();
    if (path) {
      params.set('path', path);
    }
    const data = await fetchJson(`/api/folders${params.toString() ? `?${params}` : ''}`);
    setDirectory({
      directories: data.directories || [],
      files: data.files || [],
      parent: data.parent || null
    });
    setContext((prev) => ({ rootPath: data.root, currentPath: stickToCurrent ? prev.currentPath : data.path }));
  };

  const refreshQueue = async () => {
    const data = await fetchJson('/api/queue');
    setQueue(data || []);
  };

  const refreshStatus = async () => {
    const data = await fetchJson('/api/status');
    setStatus(data || {});
  };

  const refreshResults = async () => {
    const data = await fetchJson('/api/comics');
    setResults(data || []);
  };

  const refreshModels = async (nextServer) => {
    setLoadingModels(true);
    try {
      const list = await fetchJson(`/api/models?server=${encodeURIComponent(nextServer)}`);
      setModels(list || []);
      setSelectedModel((current) => {
        if (current && list?.includes(current)) {
          return current;
        }
        return list?.[0] || '';
      });
    } catch (err) {
      setError(err.message);
    } finally {
      setLoadingModels(false);
    }
  };

  const bootstrap = async () => {
    try {
      await Promise.all([refreshContext(), loadDirectory(), refreshQueue(), refreshStatus(), refreshResults()]);
      await refreshModels(server);
    } catch (err) {
      setError(err.message);
    }
  };

  useEffect(() => {
    bootstrap();
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    const interval = setInterval(() => {
      refreshStatus();
      refreshQueue();
    }, 4000);
    return () => clearInterval(interval);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const handleAddToQueue = async (path) => {
    try {
      const response = await fetchJson('/api/queue', {
        method: 'POST',
        body: JSON.stringify({ path })
      });
      if (response?.errors?.length) {
        setError(response.errors.join('\n'));
      } else if (!response?.added) {
        setMessage('Already queued.');
      } else {
        setMessage('Added to queue.');
      }
      await refreshQueue();
    } catch (err) {
      setError(err.message);
    }
  };

  const handleRemoveFromQueue = async (path) => {
    try {
      await fetchJson(`/api/queue?path=${encodeURIComponent(path)}`, { method: 'DELETE' });
      await refreshQueue();
    } catch (err) {
      setError(err.message);
    }
  };

  const handleClearQueue = async () => {
    try {
      await fetchJson('/api/queue/all', { method: 'DELETE' });
      await refreshQueue();
    } catch (err) {
      setError(err.message);
    }
  };

  const handleProcessQueue = async () => {
    if (!selectedModel) {
      setError('Select a model before processing.');
      return;
    }
    try {
      setMessage('Processing queue...');
      setError(null);
      await fetchJson('/api/process', {
        method: 'POST',
        body: JSON.stringify({ server, model: selectedModel, generateEbay })
      });
      await Promise.all([refreshStatus(), refreshQueue(), refreshResults()]);
    } catch (err) {
      setError(err.message);
    }
  };

  const processingLabel = useMemo(() => {
    if (!status.processing) return 'Idle';
    if (status.currentItem) return `Processing ${status.currentItem}`;
    return 'Processing…';
  }, [status]);

  const handleToggleResult = (id) => {
    setExpandedResultId((current) => (current === id ? null : id));
  };

  const handleDeleteResult = async (id) => {
    if (!globalThis.confirm('Are you sure you want to delete this comic?')) return;
    try {
      await fetchJson(`/api/comics?id=${id}`, { method: 'DELETE' });
      await refreshResults();
    } catch (err) {
      setError(err.message);
    }
  };

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 p-6 space-y-6 max-w-6xl mx-auto w-full overflow-hidden">
      <header className="flex flex-col gap-3 md:flex-row md:items-center md:justify-between w-full">
        <div>
          <h1 className="text-3xl font-bold flex items-center gap-2">
            <ServerCog size={28} className="text-indigo-400" />
            ComicVision Control Center
          </h1>
          <p className="text-slate-400">Browser-first workflow. Desktop exe now powers backend processing.</p>
        </div>
        <div className={`flex items-center gap-2 px-4 py-2 rounded-xl border ${status.processing ? 'border-yellow-500/40 bg-yellow-500/10' : 'border-emerald-500/40 bg-emerald-500/10'}`}>
          {status.processing ? <Loader2 className="animate-spin" size={18} /> : <CheckCircle2 size={18} />}
          <span className="text-sm font-medium">{processingLabel}</span>
        </div>
      </header>

      {(message || error) && (
        <div className={`rounded-xl px-4 py-3 flex items-center gap-3 ${error ? 'bg-red-500/10 border border-red-500/30 text-red-300' : 'bg-emerald-500/10 border border-emerald-500/30 text-emerald-200'}`}>
          {error ? <AlertTriangle size={18} /> : <CheckCircle2 size={18} />}
          <span className="text-sm whitespace-pre-line">{error || message}</span>
          <button className="ml-auto text-xs text-slate-400" onClick={() => { setError(null); setMessage(null); }}>Dismiss</button>
        </div>
      )}

      <main className="grid gap-6 w-full max-w-full lg:grid-cols-2">
        <section className="space-y-6 w-full max-w-full">
          <div className="bg-slate-800/70 border border-slate-700 rounded-3xl p-6 space-y-5 w-full">
            <div className="flex items-center justify-between">
              <div>
                <h2 className="text-xl font-semibold flex items-center gap-2"><FolderOpen size={20} /> Browse Comics</h2>
                <p className="text-xs text-slate-400">Root: {context.rootPath || 'loading…'}</p>
              </div>
              <div className="flex items-center gap-2">
                <button
                  className="px-3 py-2 text-xs rounded-lg bg-slate-900 border border-slate-700 hover:border-indigo-400"
                  onClick={() => loadDirectory(directory.parent || context.rootPath)}
                  disabled={!directory.parent}
                >
                  <ChevronUp size={14} /> Up
                </button>
                <button
                  className="px-3 py-2 text-xs rounded-lg bg-slate-900 border border-slate-700 hover:border-indigo-400"
                  onClick={() => loadDirectory(context.currentPath, true)}
                >
                  <RefreshCw size={14} className="inline-block mr-1" /> Refresh
                </button>
              </div>
            </div>
            <div className="grid gap-4 md:grid-cols-2">
              <div className="space-y-2">
                <h3 className="text-sm font-medium text-slate-300 flex items-center gap-1"><List size={15} /> Directories</h3>
                <div className="rounded-xl border border-slate-700 bg-slate-900/60 max-h-72 overflow-y-auto divide-y divide-slate-800">
                  {directory.directories?.length ? directory.directories.map((dir) => (
                    <button
                      key={dir.path}
                      className="w-full text-left px-4 py-3 hover:bg-slate-800 flex items-center gap-2 text-sm"
                      onClick={() => loadDirectory(dir.path)}
                    >
                      <FolderOpen size={16} className="text-indigo-300" />
                      <span className="truncate">{dir.name}</span>
                    </button>
                  )) : (
                    <p className="px-4 py-3 text-xs text-slate-500">No subdirectories.</p>
                  )}
                </div>
              </div>
              <div className="space-y-2">
                <h3 className="text-sm font-medium text-slate-300 flex items-center gap-1"><ImageIcon size={15} /> Images</h3>
                <div className="rounded-xl border border-slate-700 bg-slate-900/60 max-h-72 overflow-y-auto divide-y divide-slate-800">
                  {directory.files?.length ? directory.files.map((file) => (
                    <div key={file.path} className="px-4 py-3 flex items-center gap-3 text-sm">
                      <FileText size={16} className="text-slate-400" />
                      <div className="flex-1">
                        <p className="truncate font-medium">{file.name}</p>
                        <p className="text-xs text-slate-500">{Math.round(file.size / 1024)} KB</p>
                      </div>
                      <button
                        className="text-xs px-3 py-1.5 rounded-lg bg-indigo-600 hover:bg-indigo-500"
                        onClick={() => handleAddToQueue(file.path)}
                      >
                        Queue
                      </button>
                    </div>
                  )) : (
                    <p className="px-4 py-3 text-xs text-slate-500">No images in this folder.</p>
                  )}
                </div>
              </div>
            </div>
          </div>

          <div className="bg-slate-800/70 border border-slate-700 rounded-3xl p-6 space-y-4 w-full">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold flex items-center gap-2"><List size={20} /> Processing Queue</h2>
              <div className="flex items-center gap-2 text-xs text-slate-400">
                <span>Queued: {queue.filter((item) => item.status === 'queued').length}</span>
                <span>Pending: {queue.filter((item) => item.status !== 'queued').length}</span>
              </div>
            </div>
            <div className="rounded-xl border border-slate-700 bg-slate-900/60 max-h-64 overflow-y-auto divide-y divide-slate-800">
              {queue.length ? queue.map((item) => (
                <div key={`${item.path}-${item.status}`} className="px-4 py-3 flex items-center gap-3 text-sm">
                  <div className={`w-2 h-2 rounded-full ${item.status === 'processing' ? 'bg-yellow-400 animate-pulse' : item.status === 'pending' ? 'bg-slate-400' : 'bg-indigo-400'}`} />
                  <div className="flex-1">
                    <p className="font-medium truncate">{item.name}</p>
                    <p className="text-xs text-slate-500 truncate">{item.path}</p>
                  </div>
                  {item.status === 'queued' && (
                    <button className="text-xs px-3 py-1 rounded-lg border border-slate-600 hover:border-red-400" onClick={() => handleRemoveFromQueue(item.path)}>
                      <Trash2 size={14} />
                    </button>
                  )}
                </div>
              )) : (
                <p className="px-4 py-3 text-xs text-slate-500">Queue is empty.</p>
              )}
            </div>
            <div className="flex items-center justify-between">
              <button className="px-4 py-2 text-xs rounded-lg border border-slate-600 hover:border-red-400" onClick={handleClearQueue}>
                Clear Queue
              </button>
              <button
                className="px-4 py-2 text-xs rounded-lg bg-indigo-600 hover:bg-indigo-500 flex items-center gap-2"
                onClick={handleProcessQueue}
              >
                <Play size={14} /> Process Queue
              </button>
            </div>
          </div>
        </section>

        <section className="space-y-6 w-full max-w-full">
          <div className="bg-slate-800/70 border border-slate-700 rounded-3xl p-6 space-y-5 w-full">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold flex items-center gap-2"><ServerCog size={20} /> Processing Configuration</h2>
              <button className="text-xs px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-700 hover:border-indigo-400" onClick={() => refreshModels(server)} disabled={loadingModels}>
                <RefreshCw size={14} className={loadingModels ? 'animate-spin' : ''} /> Models
              </button>
            </div>
            <div className="space-y-3">
              <label className="text-xs uppercase text-slate-500">Server</label>
              <select
                value={server}
                onChange={(event) => {
                  const next = event.target.value;
                  setServer(next);
                  refreshModels(next);
                }}
                className="w-full px-3 py-2 rounded-lg bg-slate-900 border border-slate-700 focus:border-indigo-400"
              >
                {serverOptions.map((option) => (
                  <option key={option.value} value={option.value}>{option.label}</option>
                ))}
              </select>
            </div>
            <div className="space-y-3">
              <label className="text-xs uppercase text-slate-500">Model</label>
              <select
                value={selectedModel}
                onChange={(event) => setSelectedModel(event.target.value)}
                className="w-full px-3 py-2 rounded-lg bg-slate-900 border border-slate-700 focus:border-indigo-400"
              >
                {models.length ? models.map((model) => (
                  <option key={model} value={model}>{model}</option>
                )) : (
                  <option value="">{loadingModels ? 'Loading…' : 'No models detected'}</option>
                )}
              </select>
            </div>
            <div className="rounded-xl border border-slate-700 bg-slate-900/60 p-4 text-sm text-slate-300 space-y-2">
              <div className="flex items-center gap-2">
                <ServerCog size={16} className="text-indigo-300" />
                <span>Server: {status.server || '—'}</span>
              </div>
              <div className="flex items-center gap-2">
                <List size={16} className="text-indigo-300" />
                <span>Model: {status.model || '—'}</span>
              </div>
              <div className="flex items-center gap-2">
                <ImageIcon size={16} className="text-indigo-300" />
                <span>Queue length: {status.queueLength ?? 0}</span>
              </div>
            </div>
            <label className="flex items-center gap-3 cursor-pointer select-none">
              <input
                type="checkbox"
                className="w-4 h-4 accent-indigo-500"
                checked={generateEbay}
                onChange={(e) => setGenerateEbay(e.target.checked)}
              />
              <span className="text-sm text-slate-300">Generate eBay listing title &amp; description</span>
            </label>
          </div>
        </section>

        <section className="space-y-6 w-full max-w-full lg:col-span-2">
          <div className="bg-slate-800/70 border border-slate-700 rounded-3xl p-6 space-y-4 w-full">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold flex items-center gap-2"><ImageIcon size={20} /> Processed Comics</h2>
              <button className="text-xs px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-700 hover:border-indigo-400" onClick={refreshResults}>
                <RefreshCw size={14} /> Refresh
              </button>
            </div>
            <div className="rounded-xl border border-slate-700 bg-slate-900/60 max-h-[32rem] overflow-y-auto overflow-x-auto w-full">
              <table className="w-full text-left text-sm">
                <thead className="sticky top-0 bg-slate-900/80 backdrop-blur border-b border-slate-700 text-xs uppercase text-slate-500">
                  <tr>
                    <th className="px-4 py-3 w-24">Cover</th>
                    <th className="px-4 py-3">Title</th>
                    <th className="px-4 py-3">Issue</th>
                    <th className="px-4 py-3">Publisher</th>
                    <th className="px-4 py-3">Condition</th>
                    <th className="px-4 py-3">Value</th>
                    <th className="px-4 py-3">Actions</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-800">
                  {results.length ? results.map((item) => {
                    const expanded = expandedResultId === item.id;
                    return (
                      <React.Fragment key={item.id}>
                        <tr className={`hover:bg-slate-800/50 ${expanded ? 'bg-slate-800/60' : ''}`}>
                          <td className="px-4 py-3 align-top">
                            <div className="h-20 w-16 overflow-hidden rounded-lg bg-slate-800 border border-slate-700 flex items-center justify-center">
                              <img
                                src={`${API_BASE}/image?id=${item.id}`}
                                alt={item.title || 'Processed comic'}
                                className="h-full w-full object-cover"
                                loading="lazy"
                              />
                            </div>
                          </td>
                          <td className="px-4 py-3 align-top">
                            <p className="font-medium text-slate-200 whitespace-pre-wrap break-words select-text max-w-[20rem]">
                              {item.title || 'Untitled'}
                            </p>
                            {item.notes && (
                              <p className="mt-1 text-xs text-slate-500 whitespace-pre-wrap break-words select-text">
                                {item.notes}
                              </p>
                            )}
                          </td>
                          <td className="px-4 py-3 text-slate-300 align-top">{item.issue || '—'}</td>
                          <td className="px-4 py-3 text-slate-300 align-top">{item.publisher || '—'}</td>
                          <td className="px-4 py-3 text-yellow-400 align-top">{item.condition || '—'}</td>
                          <td className="px-4 py-3 text-emerald-300 align-top">{formatCurrency(item.value)}</td>
                          <td className="px-4 py-3 align-top">
                            <div className="flex flex-wrap items-center gap-2">
                              <a
                                className="text-xs px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-700 hover:border-indigo-400"
                                href={`${API_BASE}/image?id=${item.id}`}
                                target="_blank"
                                rel="noreferrer"
                              >
                                View
                              </a>
                              <button
                                type="button"
                                onClick={() => handleToggleResult(item.id)}
                                className="text-xs px-3 py-1.5 rounded-lg border border-slate-700 hover:border-indigo-400 bg-slate-900 flex items-center gap-1"
                              >
                                {expanded ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
                                {expanded ? 'Hide' : 'Details'}
                              </button>
                              <button
                                type="button"
                                onClick={() => handleDeleteResult(item.id)}
                                className="text-xs px-3 py-1.5 rounded-lg border border-red-500/30 text-red-400 hover:bg-red-500/10 flex items-center gap-1"
                                title="Delete Result"
                              >
                                <Trash2 size={14} />
                              </button>
                            </div>
                          </td>
                        </tr>
                        {expanded && (
                          <tr className="bg-slate-900/60">
                            <td colSpan={7} className="px-6 py-4 text-sm text-slate-200">
                              <div className="flex flex-col gap-3">
                                <div className="grid gap-3 sm:grid-cols-2">
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Title</p>
                                    <p className="select-text whitespace-pre-wrap break-words">{item.title || 'Untitled'}</p>
                                  </div>
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Issue</p>
                                    <p className="select-text">{item.issue || '—'}</p>
                                  </div>
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Publisher</p>
                                    <p className="select-text">{item.publisher || '—'}</p>
                                  </div>
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Condition</p>
                                    <p className="select-text">{item.condition || '—'}</p>
                                  </div>
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Value</p>
                                    <p className="select-text">{formatCurrency(item.value)}</p>
                                  </div>
                                </div>
                                {item.notes && (
                                  <div>
                                    <p className="text-xs uppercase tracking-wide text-slate-500">Notes</p>
                                    <p className="select-text whitespace-pre-wrap break-words">{item.notes}</p>
                                  </div>
                                )}
                              </div>
                            </td>
                          </tr>
                        )}
                      </React.Fragment>
                    );
                  }) : (
                    <tr>
                      <td className="px-4 py-6 text-center text-xs text-slate-500" colSpan={7}>No processed comics yet.</td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>
        </section>
      </main>
    </div>
  );
}
