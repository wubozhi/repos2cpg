# - coding = utf-8-#
from git import Repo
import csv
import os
import sys
import time
import numpy as np
import re
import glob
from multiprocessing import Process,Manager,Lock,Pool
maxInt = sys.maxsize
decrement = True
while decrement:
    decrement = False
    try:
        csv.field_size_limit(maxInt)
    except OverflowError:
        maxInt = int(maxInt/10)
        decrement = True

class GetCommits:
	def __init__(self,params):
		self.vul_type = params['vul_type']
		self.commits_path = params['commit_path']
		for i in self.vul_type:
			if os.path.exists(self.commits_path+os.sep+i) == False:
				os.makedirs(self.commits_path+os.sep+i)		
		self.repos_file = params['repos_file']
		self.processes_number = params['processes_number']
		self.vul_num_path = params['vul_num_path']
		if os.path.exists(self.vul_num_path) == False: 
			os.makedirs(self.vul_num_path)

	def write_repo_commits_csv(self,file,message,commit_id,commit_parents_id,date,committer,diff):
		if not os.path.exists(file):
			with open(file,'w', encoding="utf8", errors='ignore',newline = "") as f:
				writer = csv.writer(f)
				writer.writerow(['message','commit_id','parent_commit','date','committer','diff'])
		with open(file,'a+', encoding="utf8", errors='ignore',newline = "") as f:
			writer = csv.writer(f)
			writer.writerow([message,commit_id,commit_parents_id,date,committer,diff])

	def count_repo_vul_number_csv(self,file,num):
		if not os.path.exists(file):
			with open(file,'w', encoding="utf8", errors='ignore',newline = "") as f:
				writer = csv.writer(f)
				writer.writerow(["type","number"])
		with open(file,'a+', encoding="utf8", errors='ignore',newline = "") as f:
			writer = csv.writer(f)
			for i,j in num.items():
				writer.writerow([i,j])
				print(str(i) + ': ' + str(j))

	def count_repos_vul_number_csv(self,path):
		vul_num=dict()
		for i in self.vul_type:
			vul_num[i]=0
		part_vul_num=dict()
		for i in self.vul_type:
			part_vul_num[i]=0

		files = os.listdir(path)
		print(files)
		for file in files:
			with open(path+os.sep+file,'r') as f:
				lines = f.readlines()
				for line in lines:
					k = line.split(',')[0]
					v = line.split(',')[1]
					if k == 'type':
						continue
					else:
						if file.split('_')[-1] == "num.csv":
							vul_num[k]=vul_num[k]+int(v)
						if file.split('_')[-1] == "partnum.csv":
							part_vul_num[k]=part_vul_num[k]+int(v)
		self.count_repo_vul_number_csv(path+os.sep+"num.csv",vul_num)
		self.count_repo_vul_number_csv(path+os.sep+"partnum.csv",part_vul_num)


	def get_commit_attributes(self,repo,commit):
		message = commit.message
		commit_id = commit.hexsha
		commit_parents_id = commit.parents[0].hexsha
		date = commit.authored_datetime
		committer = commit.committer.name
		diff = repo.git.diff(commit.parents[0],commit)
		return message,commit_id,commit_parents_id,date,committer,diff

	def get_repos_list(self):
		repos=dict()
		with open(self.repos_file,'r', encoding="utf8", errors='ignore') as file:
			for line in file:
				name = line.split(',')[3].strip()
				path = line.split(',')[4].strip()
				repos[name] = path
		return repos

	def get_commit_from_repo(self,repo_path):
		pattern = re.compile(r'@@.*?@@')

		type_part_num = dict()
		type_num = dict()

		for i in self.vul_type:
			type_part_num[i]=0
			type_num[i]=0

		repo = Repo(repo_path)
		name = repo_path.split(os.sep)[-1]
		
		n = 0
		commits_obj = list(repo.iter_commits(reverse=True)) 
		for commit in commits_obj:
			if not commit.parents:
				continue
			else:
				n = n + 1
				print("commit: "+ str(n)+ " from "+str(name))
				message,commit_id,commit_parents_id,date,committer,diff = self.get_commit_attributes(repo,commit)

				for i in self.vul_type:
					if i in message:										
						self.write_repo_commits_csv(self.commits_path+os.sep+i+os.sep+name+"_commits.csv", message,commit_id,commit_parents_id,date,committer,diff)
						type_num[i] = type_num[i] + 1
						
						if len(pattern.findall(diff))<5:
							self.write_repo_commits_csv(self.commits_path+os.sep+i+os.sep+name+"_partcommits.csv", message,commit_id,commit_parents_id,date,committer,diff)
							type_part_num[i] = type_part_num[i] + 1
					else:
						continue

		self.count_repo_vul_number_csv(self.vul_num_path+os.sep+name+"_num.csv",type_num)
		self.count_repo_vul_number_csv(self.vul_num_path+os.sep+name+"_partnum.csv",type_part_num)

	def clean_commits_before_restart(self):
		repos_read = list()
		files = os.listdir(self.vul_num_path)
		for file in files:
			name = file.split("_num.csv")[0]
			repos_read.append(name)

		for i in self.vul_type:			
			files = os.listdir(self.commits_path+os.sep+i)
			for file in files:
				name = file.split(os.sep)[-1].split('_')[0]
				try:
					if name not in repos_read:
						os.remove(self.commits_path+os.sep+i+os.sep+file)
				except Exception as e:
					continue
		return repos_read



	def get_commits(self):	
		### get repos list ##############
		repos = self.get_repos_list()

		repos_read = self.clean_commits_before_restart()

		pool = Pool(processes=self.processes_number)

		for name,repo in repos.items():
			if name in repos_read:
				continue
			else:
				pool.apply_async(self.get_commit_from_repo,(repo,))		

		pool.close()
		pool.join()

		self.count_repos_vul_number_csv(self.vul_num_path)


# if __name__ == '__main__':
