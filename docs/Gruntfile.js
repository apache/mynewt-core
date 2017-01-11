module.exports = function(grunt) {

  // load all grunt tasks
  require('matchdep').filterDev('grunt-*').forEach(grunt.loadNpmTasks);

  grunt.initConfig({
    open : {
      dev: {
        path: 'http://localhost:1919'
      }
    },

    connect: {
      server: {
        options: {
          port: 1919,
          base: '_build/html',
          livereload: true
        }
      }
    },

    exec: {
      build_sphinx: {
        cmd: 'make htmldocs'
      }
    },

    clean: {
      build: ["_build"]
    },

    watch: {
      /* Changes in docs, theme or static content */
      sphinx: {
        files: ['**/*.rst', 'themes/**/*', '_static/**/*', 'conf.py'],
        tasks: ['clean:build','exec:build_sphinx']
      },
      /* live-reload if sphinx re-builds */
      livereload: {
        files: ['_build/**/*'],
        options: { livereload: true }
      }
    }

  });

  grunt.registerTask('default', ['clean:build','exec:build_sphinx','connect','open','watch']);
  grunt.registerTask('build', ['clean:build','exec:build_sphinx']);
}
