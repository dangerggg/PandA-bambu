pipeline {
  environment {
    shouldBuild = true
  }
  agent any
  stages {
    stage ("Shall we build") {
        steps {
            script {
                result = sh (script: "git log -1 | grep 'Author: JenkinsCI.*'", returnStatus: true) 
                if (result == 0) {
                    echo ("'It's a Jenkins commit: nothing to do.")
                    shouldBuild = false
                }
            }
        }
    }
    stage('Clean-up') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        sh 'git reset --hard && git clean -fdx '
      }
    }
    stage('build') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        withEnv(['PATH+EXTRACCACHE=/usr/lib/ccache']) {
          sh 'make -f Makefile.init J=20 && mkdir build && cd build  && ../configure --prefix=$WORKSPACE/panda-bin --enable-flopoco --enable-xilinx --enable-modelsim --enable-icarus --enable-verilator --enable-altera --enable-lattice --enable-glpk --enable-release --enable-unordered --disable-asserts --enable-opt --with-gcc49=/distros/ubuntu_last_64/usr/bin/gcc-4.9 --with-gcc5=/distros/ubuntu_16-04_64/usr/bin/gcc-5 --with-mentor-license=2003@lmw-2d.polimi.it  && nice -n 17 make -j20  install  '
        }
      }
    }
    stage('tests') {
      when {
        expression {
          return shouldBuild
        }
      }
      parallel {
        stage('list based') {
          steps {
           sh 'mkdir $WORKSPACE/list && mkdir $WORKSPACE/list/test-reports && cd $WORKSPACE/panda_regressions && nice -n 17 ./panda_regression_hls.sh -j10 --bambu $WORKSPACE/panda-bin/bin/bambu --spider $WORKSPACE/panda-bin/bin/spider --junitdir="$WORKSPACE/list/test-reports" -t 120m --returnfail '
          }
        }
        stage('sdc scheduling') {
          steps {
           sh 'mkdir $WORKSPACE/sdc_tests/ && mkdir $WORKSPACE/sdc_tests/test-reports && mkdir $WORKSPACE/panda_regressions/sdc_tests && cd $WORKSPACE/panda_regressions/sdc_tests && nice -n 15 $WORKSPACE/panda_regressions/panda_regression_hls.sh -j10 --bambu $WORKSPACE/panda-bin/bin/bambu --spider $WORKSPACE/panda-bin/bin/spider --junitdir="$WORKSPACE/sdc_tests/test-reports" -t 120m -c="--speculative-sdc-scheduling" --returnfail '
          }
        }
        stage('VHDL') {
          steps {
           sh 'mkdir $WORKSPACE/panda_regressions/vhdl_tests && mkdir $WORKSPACE/vhdl_tests/ && mkdir $WORKSPACE/vhdl_tests/test-reports && cd $WORKSPACE/panda_regressions/vhdl_tests && nice -n 16 $WORKSPACE/panda_regressions/panda_regression_hls.sh -j10 --bambu $WORKSPACE/panda-bin/bin/bambu --spider $WORKSPACE/panda-bin/bin/spider --junitdir="$WORKSPACE/vhdl_tests/test-reports" -t 120m -c="-wH" --name="_VHDL" --returnfail '
          }
        }
      }
    }
    stage('Synthesis Step') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        sh 'mkdir $WORKSPACE/examples/test-reports && mkdir $WORKSPACE/examples/pp-reports && cd $WORKSPACE/examples && nice -n 17 ./example.sh -j8 --bambu $WORKSPACE/panda-bin/bin/bambu --spider $WORKSPACE/panda-bin/bin/spider --junitdir="$WORKSPACE/examples/test-reports" --perfpublisherdir="$WORKSPACE/examples/pp-reports" '
      }
    }
    stage('Publish Junits results') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        junit allowEmptyResults: true, testResults: 'vhdl_tests/test-reports/*.xml,sdc_tests/test-reports/*.xml,list/test-reports/*.xml,examples/test-reports/*.xml'
      }
    }
    stage('Publish Perf results') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        perfpublisher healthy: '0', metrics: 'AreaxTime=areatime;Slices=slices;sliceluts=sliceluts;registers=registers;dsps=dsps;brams=brams;period=period;slack=slack;frequency=frequency', name: 'examples/pp-reports/*.xml', parseAllMetrics: false, threshold: '', unhealthy: '0', unstableThreshold: ''
      }
    }
    stage('Copy data Step') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        sh 'cd $WORKSPACE/examples && cp CHStone*tex CHStone || :'
        sh 'cd $WORKSPACE/examples && cp hls_study*tex hls_study || :'
        sh 'cd $WORKSPACE/examples && cp libm*tex libm || :'
        sh 'cd $WORKSPACE/examples && cp MachSuite*tex MachSuite || :'
        sh 'cd $WORKSPACE/examples && cp softfloat*tex softfloat || :'
        sh 'cd $WORKSPACE/examples && cp omp_simd*tex omp_simd  || :'
      }
    }
    stage('Push results Step') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        withCredentials([usernamePassword(credentialsId: '91bbe76a-aa1b-465b-bcf8-3faaa27471af', passwordVariable: 'GIT_PASSWORD', usernameVariable: 'GIT_USERNAME')]) {
          sh 'git config --global user.name "JenkinsCI" '
          sh 'git config --global user.email "jenkins-ci@example.com" '
          sh 'git commit -a -m "Updated synthesis results [skip ci]"  '
          sh 'git push https://${GIT_USERNAME}:${GIT_PASSWORD}@github.com/ferrandi/PandA-bambu.git  $GIT_LOCAL_BRANCH:$CHANGE_BRANCH'
        }
      }
    }

    stage('JunitCollect') {
      when {
        expression {
          return shouldBuild
        }
      }
      steps {
        junit allowEmptyResults: true, testResults: '$WORKSPACE/test-reports/*.xml'
      }
    }
  }
}
