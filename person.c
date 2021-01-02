#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "person.h"

void readPage(FILE *fp, char *pagebuf, int pagenum)
{
	fseek(fp, PAGE_SIZE*pagenum, SEEK_SET);
	fread((void*)pagebuf, PAGE_SIZE, 1, fp);
}

void writePage(FILE *fp, const char *pagebuf, int pagenum)
{
	fseek(fp, PAGE_SIZE*pagenum, SEEK_SET);
	fwrite((void*)pagebuf, PAGE_SIZE, 1, fp);
}

void pack(char *recordbuf, const Person *p)
{
	memset(recordbuf, 0xFF, RECORD_SIZE);
	sprintf(recordbuf, "%s#%s#%s#%s#%s#%s#", p->sn, p->name, p->age, p->addr, p->phone, p->email);
}

void unpack(const char *recordbuf, Person *p)
{
	char buf[RECORD_SIZE]={0,};
	strcpy(buf, recordbuf);
	char file_value[7][RECORD_SIZE] = {0,};
	char *ptr = strtok(buf, "#");
	int i=0;

	while(ptr!=NULL)
	{
		sprintf(file_value[i],"%s",ptr);
		i++;
		ptr = strtok(NULL, "#");
	}

	strcpy(p->sn, file_value[0]);
	strcpy(p->name, file_value[1]);
	strcpy(p->age, file_value[2]);
	strcpy(p->addr, file_value[3]);
	strcpy(p->phone, file_value[4]);
	strcpy(p->email, file_value[5]);

}

void insert(FILE *fp, const Person *p)
{
	char *recordbuf=(char*)malloc(RECORD_SIZE);
	char *pagebuf=(char*)malloc(PAGE_SIZE);
	char *header_pagebuf=(char*)malloc(PAGE_SIZE);
	int delete_page; //삭제된 레코드의 페이지 번호
	int delete_record; //삭제된 레코드의 페이지 안에서의 번호
	int total_page; //전체 페이지 수
	int total_record; //전체 레코드 수
	int insert_page=-1; 
	int insert_record=-1;

	memset(recordbuf, 0xFF, RECORD_SIZE); //초기화
	memset(pagebuf, 0xFF, PAGE_SIZE);
	memset(header_pagebuf, 0xFF, PAGE_SIZE);

	pack(recordbuf, p); //레코드 만들기

	readPage(fp, header_pagebuf, 0); //헤더 페이지 확인
	memcpy(&total_page, header_pagebuf, sizeof(int));
	memcpy(&total_record, header_pagebuf+4, sizeof(int));
	memcpy(&delete_page,header_pagebuf+8,sizeof(int));
	memcpy(&delete_record, header_pagebuf+12, sizeof(int));

	
	if(delete_page==-1&&delete_record==-1){ //삭제된 레코드가 없다면
		if(total_page>1){ //이미 다른 레코드 파일이 존재한다면
			
			readPage(fp, pagebuf, total_page-1);
			for(int i=0; (PAGE_SIZE-(i*RECORD_SIZE))>RECORD_SIZE; i++){ //가장 최근에 사용된 페이지에서 레코드를 저장할 수 있는지 확인
				char *c = (char*)malloc(sizeof(char));
				char *fc = (char*)malloc(sizeof(char));
				memset(fc, 0xFF, sizeof(char));
				memcpy(c, pagebuf+(i*RECORD_SIZE), 1);

				if(strcmp(c, fc)==0){ //사용할 수 있다면 해당 페이지와 위치 저장
					insert_page = total_page-1;
					insert_record = i;
					memcpy(pagebuf+(i*RECORD_SIZE), recordbuf, RECORD_SIZE); //페이지에 레코드 쓰기 
					writePage(fp, pagebuf, total_page-1); //레코드 파일에 해당 페이지 쓰기
					free(c);
					free(fc);

					/*헤더페이지 업데이트*/
					total_record++;
					memcpy(header_pagebuf+4, &total_record, sizeof(int));
					writePage(fp, header_pagebuf, 0);
					break;
				}
			}
		}

		if(insert_page==-1){ //새로운 페이지를 할당해야한다면
			memset(pagebuf, 0xFF, PAGE_SIZE);
			memcpy(pagebuf, recordbuf, RECORD_SIZE);
			writePage(fp, pagebuf, total_page);

			/* 헤더 페이지 업데이트 */
			total_page++;
			total_record++;
			memcpy(header_pagebuf, &total_page, sizeof(int));
			memcpy(header_pagebuf+4, &total_record, sizeof(int));
			writePage(fp, header_pagebuf, 0);
		}
	}

	else{ //삭제된 레코드가 존재한다면
		readPage(fp, pagebuf, delete_page); //삭제된 페이지 가져오기
		int old_delete_page; //더 오래전 삭제된 레코드
		int old_delete_record;
		memcpy(&old_delete_page, pagebuf+(delete_record*RECORD_SIZE)+1, sizeof(int));
		memcpy(&old_delete_record, pagebuf+(delete_record*RECORD_SIZE)+5, sizeof(int));
		memcpy(pagebuf+(delete_record*RECORD_SIZE), recordbuf, RECORD_SIZE);
		writePage(fp, pagebuf, delete_page); //해당 페이지에 레코드 저장 후, 레코드 파일에 그 페이지 다시 쓰기
		
		/*헤더 페이지 업데이트*/
		memcpy(header_pagebuf+8, &old_delete_page, sizeof(int));
		memcpy(header_pagebuf+12, &old_delete_record, sizeof(int));
		writePage(fp, header_pagebuf, 0);
	}

	free(recordbuf);
	free(pagebuf);
	free(header_pagebuf);
}

void delete(FILE *fp, const char *sn)
{
	Person p;
	char *recordbuf=(char*)malloc(RECORD_SIZE);
	char *pagebuf=(char*)malloc(PAGE_SIZE);
	char *header_pagebuf=(char*)malloc(PAGE_SIZE);
	int delete_page; //삭제된 레코드의 페이지 번호
	int delete_record; //삭제된 레코드의 페이지 안에서의 번호
	int total_page; //전체 페이지 수
	int total_record; //전체 레코드 수
	int new_deletepage = -1;
	int new_deleterecord = -1;

	memset(recordbuf, 0xFF, RECORD_SIZE); //초기화
	memset(pagebuf, 0xFF, PAGE_SIZE);
	memset(header_pagebuf, 0xFF, PAGE_SIZE);

	readPage(fp, header_pagebuf, 0); //헤더 페이지 확인
	memcpy(&total_page, header_pagebuf, sizeof(int));
	memcpy(&total_record, header_pagebuf+4, sizeof(int));
	memcpy(&delete_page,header_pagebuf+8,sizeof(int));
	memcpy(&delete_record, header_pagebuf+12, sizeof(int));

	for(int i=1; i<total_page; i++){ //각각의 페이지 가져오기
		readPage(fp, pagebuf, i);
		for(int j=0; (PAGE_SIZE-(j*RECORD_SIZE))>RECORD_SIZE; j++){ //입력받은 주민번호와 일치하는 레코드 찾기
			memcpy(recordbuf, pagebuf+(j*RECORD_SIZE), RECORD_SIZE);
			unpack(recordbuf, &p);
			if(strcmp(p.sn, sn)==0){ //일치하는 레코드 발견시 삭제
				new_deletepage = i;
				new_deleterecord = j;
				memcpy(recordbuf, "*", 1);
				memcpy(recordbuf+1, &delete_page, sizeof(int));
				memcpy(recordbuf+5, &delete_record, sizeof(int));
				memcpy(pagebuf+(j*RECORD_SIZE), recordbuf, RECORD_SIZE);
				writePage(fp, pagebuf, i);

				/*헤더페이지 업데이트*/
				memcpy(header_pagebuf+8, &new_deletepage, sizeof(int));
				memcpy(header_pagebuf+12, &new_deleterecord, sizeof(int));
				writePage(fp, header_pagebuf, 0);
				break;
			}
		}
		if(new_deletepage!=-1)
			break;
	}

	if(new_deletepage==-1) //일치하는 레코드를 찾지 못했다면
		fprintf(stderr, "입력하신 주민번호와 일치하는 레코드가 존재하지 않습니다.\n");

	free(recordbuf);
	free(pagebuf);
	free(header_pagebuf);
}

int main(int argc, char *argv[])
{
	FILE *fp; //레코드 파일의 파일 포인터
	char *recordbuf=(char*)malloc(RECORD_SIZE);
	char *pagebuf=(char*)malloc(PAGE_SIZE);
	char *header_pagebuf=(char*)malloc(PAGE_SIZE);
	int first_number = -1;
	Person p;

	memset(recordbuf, 0xFF, RECORD_SIZE); //초기화
	memset(pagebuf, 0xFF, PAGE_SIZE);
	memset(header_pagebuf, 0xFF, PAGE_SIZE);

	if(argc<4){ //인자 갯수 확인
		fprintf(stderr, "옵션과 필드 값을 입력해주세요.\n");
		return 0;}

	if((strcmp(argv[1],"i")!=0)&&strcmp(argv[1],"d")!=0){ //옵션 확인
		fprintf(stderr, "사용할 수 있는 옵션은 i(insert), d(delete)입니다.\n");
		return 0;
	}

	if((strcmp(argv[1], "i")==0&&argc!=9)||strcmp(argv[1],"d")==0&&argc!=4){ //인자 확인
		fprintf(stderr, "옵션에 맞는 필드값을 입력해주세요.\n");
		exit(1);
	}

	if((fp=fopen(argv[2], "r+"))==NULL){ //레코드파일 오픈
		fp=fopen(argv[2], "w"); //파일이 없다면 생성
		fclose(fp);
		fp=fopen(argv[2], "r+"); //닫고 읽기 쓰기용으로 다시 오픈
	}

	if(strcmp(argv[1], "i")==0){ //레코드 삽입

		if(strlen(argv[3])>13||strlen(argv[4])>17||strlen(argv[5])>3||strlen(argv[6])>21||strlen(argv[7])>15||strlen(argv[8])>25){
			fprintf(stderr, "입력하신 필드값이 레코드의 필드값보다 큽니다.\n");
			return 0;}

		fseek(fp, 0, SEEK_END);
		if(ftell(fp)==0){ //레코드를 처음 insert하는 경우 헤더페이지 만들기
			int num = 1;
			int n= 0;
			memcpy(header_pagebuf, &num, sizeof(int));
			memcpy(header_pagebuf+4, &n, sizeof(int));
			memcpy(header_pagebuf+8, &first_number, sizeof(int));
			memcpy(header_pagebuf+12, &first_number, sizeof(int));
			writePage(fp, header_pagebuf, 0);
		}

		strcpy(p.sn, argv[3]); //주민번호
		strcpy(p.name, argv[4]); //이름
		strcpy(p.age, argv[5]); //나이
		strcpy(p.addr, argv[6]); //주소
		strcpy(p.phone, argv[7]); //번호
		strcpy(p.email, argv[8]); //이메일주소
		
		insert(fp, &p);
	}

	else if(strcmp(argv[1], "d")==0){ //레코드 삭제
		if(strlen(argv[3])>13){
			fprintf(stderr, "입력하신 필드값이 레코드의 필드값보다 큽니다.\n");
			return 0;}
		delete(fp,argv[3]);
	}

	else //다른 옵션 선택 시 에러
		fprintf(stderr, "사용할 수 있는 옵션은 i(insert), d(delete)입니다.\n");

	return 1;
}
