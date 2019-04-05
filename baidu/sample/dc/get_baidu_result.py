import os,sys
import logging
import time

logger=logging.getLogger(__name__)
name=sys.argv[1]
asr=sys.argv[2]

files_path='/home/wangjinyu/workproject/long_audio_asr/res/real_wav/'
files_path=os.path.join(files_path,name)
files_list=os.listdir(files_path)
from bai_api import get_result


text_name=asr+'.txt'
text_path=os.path.join('/home/wangjinyu/workproject/long_audio_asr/res/real_test/{}/txt'.format(name),text_name)
f=open(text_path,'a')
s_time=time.time()
for i in range(len(files_list)):
    file_path=os.path.join(files_path,name+str(i+1)+'.wav')
    try:
        res=get_result(file_path)
    except:

        f.write('\n')
        logging.exception('识别失败')
        print(file_path + '识别失败')
    else:
        f.write(res+'\n')
        print(file_path+'识别成功')
e_time=time.time()
t_time=e_time-s_time

with open('/home/wangjinyu/workproject/long_audio_asr/time.txt','a') as fp:
    fp.write('百度识别{}消耗时间为{}'.format(sys.argv[1],t_time)+'\n')



