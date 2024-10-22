require 'open3'

requestors = { "gvrose8192" => "" }

def file_prepend(file, str)
  new_contents = ""
  File.open(file, 'r') do |fd|
    contents = fd.read
    new_contents = str << contents
  end
  # Overwrite file but now with prepended string on it
  File.open(file, 'w') do |fd| 
    fd.write(new_contents)
  end
end

def process_git_request(fname, target_branch, source_branch, prj_dir)
  retcode = 200 #presume success
#  puts "Opening file " + fname
  file = File.new(fname, "w")
  working_dir = prj_dir
#  puts "Working Dir : " + working_dir
  Dir.chdir working_dir
#  puts "pwd : " + Dir.pwd
  git_cmd = "git log --oneline --no-abbrev-commit origin/" + target_branch + ".." + "origin/" + source_branch
#  puts git_cmd
  out, err, status = Open3.capture3(git_cmd)
  if status.exitstatus != 0
    puts "Command error output is " + err
    file.write("Command error output is " + err)
    file.close
    retcode = 201
    return retcode
  end
  output_lines = out.split(' ')
# we just want the commit sha IDs
  output_lines.each { |x|
#    puts "This is output_lines " + x
    upstream_diff = false
    if !x[/\H/]
      if x.length < 40
        next
      end
      git_cmd = "git show " + x
      gitlog_out, gitlog_err, gitlog_status = Open3.capture3(git_cmd)
      if gitlog_status.exitstatus != 0
        file.write("git show command error output is " + gitlog_err)
        retcode = 201
      end
      loglines = gitlog_out.lines.map(&:chomp)
      lines_counted = 0
      local_diffdiff_sha = ""
      upstream_diffdiff_sha = ""
      loglines.each { |logline|
        lines_counted = lines_counted + 1
        if lines_counted == 1
            local_commit_sha = logline.match("[0-9a-f]\{40\}")
            local_diffdiff_sha = local_commit_sha.to_s
#            puts "Local : " + local_diffdiff_sha
            file.write("Merge Request sha: " + local_diffdiff_sha)
            file.write("\n")
        end
        if lines_counted == 2 #email address
          if !logline.downcase.include? "ciq.com"
            # Bad Author
            s = "error:\nBad " + logline + "\n"
            puts s
            file.write(s)
            retcode = 201
          else
            file.write("\t" + logline + "\n")
          end
        end
        if lines_counted > 1
          if logline.downcase.include? "jira"
            file.write("\t" + logline + "\n")
          end
          if logline.downcase.include? "upstream-diff"
            upstream_diff = true
          end
          if logline.downcase.include? "commit"
            commit_sha = logline.match("[0-9a-f]\{40\}")
            upstream_diffdiff_sha = commit_sha.to_s
#            puts "Upstream : " + upstream_diffdiff_sha
            if (!upstream_diffdiff_sha.empty?)
              file.write("\tUpstream sha: " + upstream_diffdiff_sha)
              file.write("\n")
            end
          end
        end
        if lines_counted > 8 #Everything we need should be in the first 8 lines
          break
        end
      }
      if !local_diffdiff_sha.empty? &&  !upstream_diffdiff_sha.empty?
        diff_cmd = Dir.pwd + "/.github/workflows/diffdiff.py --colour --commit " + local_diffdiff_sha
        puts "diffdiff: " + diff_cmd
        diff_out, diff_err, diff_status = Open3.capture3(diff_cmd)
        if diff_status.exitstatus != 0 && !upstream_diff
          puts "diffdiff out: " + diff_out
          puts "diffdiff err: " + diff_err
          retcode = 201
          file.write("error:\nCommit: " + local_diffdiff_sha + " differs with no upstream tag in commit message\n")
        end
      end
    end
  }
  file.close
  return retcode
end

first_arg,  *argv_in = ARGV
if argv_in.length < 5
  puts "Not enough arguments: fname, target_branch, source_branch, prj_dir, pull_request, requestor"
  exit
end
fname = first_arg.to_s
fname = "tmp-" + fname
# puts "filename is " + fname
target_branch  = argv_in[0].to_s
# puts "target branch is " + target_branch
source_branch = argv_in[1].to_s
# puts "source branch is " + source_branch
prj_dir = argv_in[2].to_s
# puts "project dir is "  + prj_dir
pullreq = argv_in[3].to_s
# puts "pull request is " + pullreq
requestor = argv_in[4].to_s
retcode = process_git_request(fname, target_branch, source_branch, prj_dir)
if retcode != 200
  File.open(fname, 'r') do |fd|
    contents = fd.read
    puts contents
  end
  exit(1)
else
  puts "Done"
end
exit(0)

