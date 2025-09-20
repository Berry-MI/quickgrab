/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  indi.wookat.quickgrab.exception.GlobalExceptionHandler
 *  org.springframework.http.HttpStatus
 *  org.springframework.http.HttpStatusCode
 *  org.springframework.http.ResponseEntity
 *  org.springframework.web.bind.annotation.ControllerAdvice
 *  org.springframework.web.bind.annotation.ExceptionHandler
 *  org.springframework.web.multipart.MaxUploadSizeExceededException
 */
package indi.wookat.quickgrab.exception;

import org.springframework.http.HttpStatus;
import org.springframework.http.HttpStatusCode;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ControllerAdvice;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.multipart.MaxUploadSizeExceededException;

@ControllerAdvice
public class GlobalExceptionHandler {
    @ExceptionHandler(value={MaxUploadSizeExceededException.class})
    public ResponseEntity<String> handleMaxFileSizeException(MaxUploadSizeExceededException ex) {
        String errorMessage = "File size exceeds the maximum allowed limit.";
        return new ResponseEntity((Object)errorMessage, (HttpStatusCode)HttpStatus.BAD_REQUEST);
    }
}

