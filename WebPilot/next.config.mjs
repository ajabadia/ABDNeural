import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dirname = path.dirname(fileURLToPath(import.meta.url));

const nextConfig = {
  output: 'export',
  trailingSlash: true,
  images: {
    unoptimized: true,
  },
  turbopack: {
    // Root of the SUITE (D:/desarrollos/ABDSynths): that is where the real
    // pnpm-workspace.yaml lives and where the symlinked shared packages
    // (@abdsynths/shared, @abdsynths/midi-keyb) physically are. With a smaller
    // root Turbopack refuses modules whose realpath falls outside it, and the
    // shared imports break. Also silences "ignored pnpm-workspace.yaml outside
    // the Git repository" — the root is now explicit.
    root: path.resolve(dirname, '../..'),
  },
};

export default nextConfig;
