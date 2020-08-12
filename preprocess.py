# - coding = utf-8-#
from repo2commits import GetCommits 
from commits2func import Getfunc
import os
# import func2cpg

def initial_params():
	params=dict()
	params['vul_type']=[
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
	params['commit_path']='commits'
	params['repos_file'] = 'c_repos_list.csv'
	params['processes_number'] = 10
	params['vul_num_path'] = 'vul_num'

	params['functions_extracted_commits'] = 'functions_extracted_commits'
	params['diffs_used_commits'] = 'diffs_used_commits'
	params['filter_diffs_num'] = 10
	params['filter_chunks_num'] = 5
	params['commits_path'] = '/home/lingling/wubozhi/datapreprocess/preprocess/commits/'

	return params

def get_commits_from_repos(params):
	GetCommits(params).get_commits()

def generate_functions_from_commits(params):
	Getfunc(params).generate_functions()




if __name__ == '__main__':

	params = initial_params()

	get_commits_from_repos(params)

	for i in params['vul_type']:
		print('\n \n start vulnerability: %s' % i)
		params['commits_path'] = os.path.join(params['commits_path'], i)
		generate_functions_from_commits(params)
		params['commits_path'] = params['commits_path'].split(i)[0]
