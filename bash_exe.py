import subprocess

# class bash_exe:
# 	def __init__(self):
# 		pass

def execute_command(cmdlist,repopath,out,err):
	cmd = " ".join(cmdlist)
	return_code = subprocess.Popen('cd '+ repopath + ' && ' +cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	status = return_code.returncode
	output, error = return_code.communicate()
	with open(out,'wb') as f:
		f.write(output)
	with open(err,'wb') as f:
		f.write(error)

# if __name__ == '__main__':
# 	be = bash_exe()
# 	cmd = ['ls','-l']
# 	stdout = 'stdout.txt'
# 	stderr = 'stderr.txt'
# 	be.execute_command(cmd,"/home/lingling/wubozhi/datapreprocess/commits",stdout,stderr)