import sys
import subprocess
import gzip
import time
import os
import pandas as pd

class CommandError(Exception):
    """
    Raised when an external command failed.

    Attributes:
        returncode: The return code from the command
        cmd: The command run
        log: The log filename (if applicable)
    """
    def __init__(self, msg, cmd, returncode, log=None):
        super(CommandError, self).__init__(msg)
        self.returncode = returncode
        self.cmd = cmd
        self.log = log


class CommandRunner(object):

    def __init__(self, timeout_sec=None, max_memory_mb=None, track_memory=True, verbose=False, echo_cmd=False, indent="\t"):
        """
        An object for running system commands with timeouts, memory limits and varying verbose-ness

        Arguments
        =========
            timeout_sec: maximum walk-clock-time of the command in seconds. Default: None
            max_memory_mb: maximum memory usage of the command in megabytes (if supported). Default: None
            track_memory: Whether to track usage of the command (disabled if not supported). Default: True
            verbose: Produce more verbose output. Default: False
            echo_cmd: Echo the command before running. Default: False
            indent: The string specifying a single indent (used in verbose mode)
        """
        self._timeout_sec = timeout_sec
        self._max_memory_mb = max_memory_mb
        self._track_memory = track_memory
        self._verbose = verbose
        self._echo_cmd = echo_cmd
        self._indent = indent

    def run_system_command(self, cmd, work_dir='.', log_filename=None, exepcted_return_code=0, indent_depth=0):
        """
        Runs the specified command in the system shell.

        Returns
        =======
            A tuple of the command output (list of lines) and the command's return code.

        Arguments
        =========
            cmd: list of tokens that form the command to be run
            log_filename: name of the log file for the command's output. Default: derived from command
            work_dir: The directory to run the command in. Default: None (uses object default).
            expected_return_code: The expected return code from the command. If the actula return code does not match, will generate an exception. Default: 0
            indent_depth: How deep to indent the tool output in verbose mode. Default 0
        """
        #Save the original command
        orig_cmd = cmd

        #If no log file is specified the name is based on the executed command
        if log_filename == None:
            log_filename = os.path.basename(orig_cmd[0]) + '.out'


        #Limit memory usage?
        memory_limit = ["ulimit", "-Sv", "{val};".format(val=self._max_memory_mb)]
        if self._max_memory_mb != None and self.check_command(memory_limit[0]):
            cmd = memory_limit + cmd

        #Enable memory tracking?
        memory_tracking = ["/usr/bin/env", "time", "-v"]
        if self._track_memory and self.check_command(memory_tracking[0]):
            cmd = memory_tracking + cmd

        #Flush before calling subprocess to ensure output is ordered
        #correctly if stdout is buffered
        sys.stdout.flush()

        #Echo the command?
        if self._echo_cmd:
            #print ' '.join(cmd)
            print cmd

        #Begin timing
        start_time = time.time()

        cmd_output=[]
        cmd_returncode=None
        proc = None
        try:
            #Call the command
            proc = subprocess.Popen(cmd,
                                    stdout=subprocess.PIPE, #We grab stdout
                                    stderr=subprocess.STDOUT, #stderr redirected to stderr
                                    universal_newlines=True, #Lines always end in \n
                                    cwd=work_dir, #Where to run the command
                                    )

            # Read the output line-by-line and log it
            # to a file.
            #
            # We do this rather than use proc.communicate()
            # to get interactive output
            with open(os.path.join(work_dir, log_filename), 'w') as log_f:
                #Print the command at the top of the log
                print >> log_f, " ".join(cmd)

                #Read from subprocess output
                for line in proc.stdout:

                    #Send to log file
                    print >> log_f, line,
                    log_f.flush()

                    #Save the output
                    cmd_output.append(line)

                    #Send to stdout
                    if self._verbose:
                        print indent_depth*self._indent + line,
                        sys.stdout.flush()

                    #Abort if over time limit
                    elapsed_time = time.time() - start_time
                    if self._timeout_sec and elapsed_time > self._timeout_sec:
                        proc.terminate()

                #Should now be finished (since we stopped reading from proc.stdout),
                #need to wait to set the return code
                proc.wait()

        finally:
            #Clean-up if we did not exit cleanly
            if proc:
                if proc.returncode == None:
                    #Still running, stop it
                    proc.terminate()

                cmd_returncode = proc.returncode

        if exepcted_return_code != None and cmd_returncode != exepcted_return_code:
            raise CommandError(msg="Executable {exec_name} failed".format(exec_name=os.path.basename(orig_cmd[0])),
                               cmd=cmd,
                               log=os.path.join(work_dir, log_filename),
                               returncode=cmd_returncode)

        return cmd_output, cmd_returncode

    def check_command(self, command):
        """
        Return True if command can be run, False otherwise.
        """

        #TODO: actually check for this
        return True

def load_histogram_csv(filename):
    print "Loading " + filename + "..."
    return pd.read_csv(filename).sort_values(by="delay")


def load_exhaustive_csv(filename):
    print "Loading " + filename + "..."
    raw_data = pd.read_csv(filename)

    return transitions_to_histogram(raw_data)

def load_trans_csv(filename):
    base, ext = os.path.splitext(filename)

    if ext == "gz":
        with gzip.open(filename, 'rb') as f:
            return pd.read_csv(f)
    else:
        return pd.read_csv(filename)

def transitions_to_histogram(raw_data):
    #Counts of how often all delay values occur
    raw_counts = raw_data['delay'].value_counts(sort=False)
    
    #Normalize by total combinations (i.e. number of rows)
    #to get probability
    normed_counts = raw_counts / raw_data.shape[0]

    df = pd.DataFrame({"delay": normed_counts.index, "probability": normed_counts.values})

    #Is there a zero probability entry?
    if not df[df['delay'] == 0.].shape[0]:
        #If not, add a zero delay @ probability zero if none is recorded
        #this ensures matplotlib draws the CDF correctly
        zero_delay_df = pd.DataFrame({"delay": [0.], "probability": [0.]})
        
        df = df.append(zero_delay_df)

    return df.sort_values(by="delay")
