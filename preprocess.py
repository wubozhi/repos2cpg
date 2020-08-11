# - coding = utf-8-#
from repo2commits import GetCommits 
from commits2func import Getfunc
import os
# import func2cpg

class Preprocess:
	def __init__(self):
		pass

	def get_commits_from_repos(self,params1):
		GetCommits(params1).repos2commits()

	def generate_functions_from_commits(self,params2):
		Getfunc(params2).generate_functions()




if __name__ == '__main__':

	vultype = [
	'injection',
	'buffer overflow',
	'out of bounds',
	'race conditions',
	'memory leak',
	'format string',
	'information leak',
	'double free',
	'use after free',
	'divide by zero',
	'off by one',
	'infinite loop',
	'integer overflow',
	'integer underflow',
	'null pointer'
	]

	params1=dict()
	params1['vul_type']=vultype
	params1['commit_path']='commits'
	params1['repos_file'] = 'c_repos_list.csv'
	params1['processes_number'] = 10
	params1['vul_num_path'] = 'vul_num'

	# Preprocess().get_commits_from_repos(params1)

	commits_path= '/home/lingling/wubozhi/datapreprocess/preprocess/commits/'
	params2=dict()
	params2['repos_file'] = 'c_repos_list.csv'
	params2['functions_extracted_commits'] = 'functions_extracted_commits'
	params2['diffs_used_commits'] = 'diffs_used_commits'
	params2['filter_diffs_num'] = 10
	params2['filter_chunks_num'] = 5

	for i in vultype:
		print('\n \n start vulnerability: %s' % i)
		params2['commits_path'] = os.path.join(commits_path, i)
		Preprocess().generate_functions_from_commits(params2)
