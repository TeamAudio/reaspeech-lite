import * as esbuild from 'esbuild';

const args = process.argv.slice(2);
const watch = args.includes('--watch');

// Define build configuration
const buildOptions = {
  entryPoints: ['src/App.ts'],
  bundle: true,
  outfile: '../../assets/js/main.js',
  format: 'iife',
  target: 'es2019',
  minify: false,
  sourcemap: true,
};

// Helper function to get formatted timestamp
const getTimestamp = () => {
  const now = new Date();
  return now.toLocaleTimeString();
};

// Function to log build events
const logBuild = (eventType) => {
  console.log(`[${getTimestamp()}] ${eventType}`);
};

if (watch) {
  // Watch mode
  esbuild.context({
    ...buildOptions,
    plugins: [{
      name: 'rebuild-notifier',
      setup(build) {
        let isFirstBuild = true;

        build.onEnd(result => {
          if (result.errors.length > 0) {
            logBuild(`Build failed with ${result.errors.length} error(s)`);
            return;
          }

          if (isFirstBuild) {
            logBuild('Initial build completed');
            isFirstBuild = false;
          } else {
            logBuild('Rebuild succeeded');
          }
        });
      }
    }]
  }).then(async (ctx) => {
    logBuild('Watching for changes...');

    // Start watching - this will trigger the initial build
    await ctx.watch();

    // Keep the process alive
    process.stdin.on('close', () => {
      ctx.dispose();
      process.exit(0);
    });

    console.log('Press Ctrl+C to stop watching');

  }).catch((error) => {
    logBuild(`Context creation failed: ${error.message}`);
    process.exit(1);
  });
} else {
  // Build once
  esbuild.build(buildOptions)
    .then(() => logBuild('Build completed'))
    .catch((error) => {
      logBuild(`Build failed: ${error.message}`);
      process.exit(1);
    });
}
