//------------------------------------------------------------------
// fpcalc_jni.c
//------------------------------------------------------------------
// Routines specific to fpcalc JNI libraries
//
// Implementation of jni_output() and the
// library_methods array and size for generic
// JNI_OnLoad() implemented in jni_utils.c
//
//-----------------------------------------------------------------
//
// This file is distributed under the GNU GENERAL PUBLIC LICENSE version 2.
// Please see the file `COPYING.txt` for details.
//
// You are free to modify it, and distribute in any way you see fit,
// as long as you retain this notification and all copyrights herein.
//
// This software is provided without any warranty, explicit or implied.
//
// (c) Copyright 2015 - Patrick Horton


#include <jni.h>
#include <stdlib.h>

//-------------------------------------------------
// interface to generic jni_utils JNI_Onload()
//-------------------------------------------------

JNIEXPORT jstring JNICALL fpCalc(JNIEnv *env, jobject obj, jobjectArray args);
	// forward declaration

JNINativeMethod library_methods[] = {
	{"fpCalc", "([Ljava/lang/String;)Ljava/lang/String;", (void*)fpCalc } };

jsize library_methods_size = sizeof(library_methods) / sizeof(JNINativeMethod);


//-----------------------------------------
// jni_output()
//-----------------------------------------

static char *retval = NULL;;

char * append_string(char * old, const char * new)
{
	const size_t old_len = old ? strlen(old) : 0;
	const size_t new_len = strlen(new);
	const size_t out_len = old_len + new_len + 1;
	char *out = malloc(out_len);
	if (old_len) memcpy(out, old, old_len);
	memcpy(out + old_len, new, new_len + 1);
	if (old_len) free(old);
	return out;
}

void jni_output(const char* format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	int size = vsnprintf(NULL, 0, format, argptr);
	char buffer[size + 1];
	vsprintf(buffer, format, argptr);
	retval = append_string(retval,buffer);
	va_end(argptr);
}


//-------------------------------------------------
// fpCalc()
//-------------------------------------------------

// JNIEXPORT jstring JNICALL Java_prh_testfpcalc_MainActivity_fpCalc
	// old funky "explicit" declaration requiring library to know
	// java class it will be linked to. See breakthrough jni_utils.c
	// for a way to get around that, as used by this library.

JNIEXPORT jstring JNICALL fpCalc(JNIEnv *env, jobject obj, jobjectArray args)
{
	int i;
	int argc = (*env)->GetArrayLength(env, args) + 1;
	char **argv = malloc(argc * sizeof(char *));
	argv[0] = (char *) "fpCalc";
	retval = NULL;

	for (i=1; i<argc; i++)
	{
		jstring js = (*env)->GetObjectArrayElement(env,args,i-1);
		char *cs = (char *)(*env)->GetStringUTFChars(env, js, 0);
		argv[i] = malloc(strlen(cs) + 1);
		strcpy(argv[i],cs);
		(*env)->ReleaseStringUTFChars(env, js, cs);
		(*env)->DeleteLocalRef(env,js);
	}

	int rslt = fpcalc_main(argc,argv);
	if (rslt == 1)
	{
		jni_output("error_fpcalc_main=%d\n",rslt);
	}

	jstring final_result_value = (*env)->NewStringUTF(env,retval);
	free(retval);
	retval = NULL;

	for (i=1; i<argc; i++)
	{
		free(argv[i]);
	}
	free(argv);

	return final_result_value;
}
